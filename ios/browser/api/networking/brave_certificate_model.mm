/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */

#import "brave_certificate_model.h"

#include "base/memory/ref_counted.h"
#include "base/strings/string_piece.h"
#include "base/time/time.h"
#include "net/cert/x509_certificate.h"
#include "net/cert/x509_cert_types.h"
#include "net/cert/x509_util_ios.h"
#include "net/cert/x509_util.h"
#include "third_party/boringssl/src/include/openssl/pool.h"
#include "third_party/boringssl/src/include/openssl/sha.h"
#include "third_party/boringssl/src/include/openssl/evp.h"

#include "net/cert/internal/cert_errors.h"
#include "net/base/net_export.h"
#include "net/cert/internal/certificate_policies.h"
#include "net/cert/internal/parse_certificate.h"
#include "net/cert/internal/parsed_certificate.h"
#include "net/cert/internal/signature_algorithm.h"
#include "net/cert/internal/verify_name_match.h"
#include "net/der/input.h"
#include "net/cert/internal/parse_name.h"
#include "net/der/encode_values.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/der/parse_values.h"
#include "net/der/parser.h"
#include "net/der/tag.h"
#include "net/cert/internal/verify_signed_data.h"
#include "net/cert/internal/common_cert_errors.h"
#include "net/cert/signed_certificate_timestamp.h"
#include "net/cert/ct_objects_extractor.h"
#include "net/cert/ct_serialization.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#include <type_traits>

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

// MARK: - Implementation

namespace x509_utils {
std::string hex_string_from_bytes(const std::uint8_t* bytes, std::size_t size) {
  static const char hex_characters[] = "0123456789ABCDEF";
  
  std::string result;
  for (std::size_t i = 0; i < size; ++i) {
    result += hex_characters[(bytes[i] & 0xF0) >> 4];
    result += hex_characters[(bytes[i] & 0x0F) >> 0];
  }
  return result;
}

std::string hex_string_from_string(const std::string& string) {
  return hex_string_from_bytes(reinterpret_cast<const std::uint8_t*>(&string[0]), string.size());
}

std::vector<std::string> split_string(const std::string& value, char separator) {
    std::vector<std::string> result;
    std::string::size_type pBeg = 0;
    std::string::size_type pEnd = value.find(separator);
    
    while(pEnd != std::string::npos) {
      result.emplace_back(value, pBeg, pEnd - pBeg);
      pBeg = pEnd + 1;
      pEnd = value.find(separator, pBeg);
    }
    result.emplace_back(value, pBeg);
    return result;
}

std::vector<std::string> split_string_into_parts(const std::string& value, std::uint32_t parts) {
  std::vector<std::string> result;
  std::size_t size = value.size() / parts;
  
  for (std::size_t i = 0; i < size; ++i) {
    result.emplace_back(value, i * parts, parts);
  }
  
  if (value.size() % parts != 0) {
    result.emplace_back(value, parts * size);
  }
  return result;
}

std::vector<std::uint8_t> absolute_oid_to_oid(const std::string& oid) {
  std::vector<std::uint32_t> list;
  for (std::string value : split_string(oid, '.')) {
    errno = 0;
    char* pEnd = nullptr;
    unsigned long int result = std::strtoul(&value[0], &pEnd, 10);
    bool success = pEnd != &value[0] &&
                   errno == 0 &&
                   pEnd &&
                   *pEnd == '\0';
    if (success && result <= std::numeric_limits<std::uint32_t>::max()) {
      list.push_back(static_cast<std::uint32_t>(result));
    } else {
      return {};  // We don't support larger than 32-bits per arc as it would require big-int.
    }
  }
  
  auto encode_octet_as_septet = [](std::uint32_t octet) -> std::vector<std::uint8_t> {;
    std::vector<std::uint8_t> encoded;
    std::uint8_t value = 0x00;
    while (octet >= 0x80) {
      encoded.insert(encoded.begin(), (octet & 0x7f) | value);
      octet >>= 7;
      value = 0x80;
    }
    encoded.insert(encoded.begin(), octet | value);
    return encoded;
  };
  
  // Invalid encoding for the root arcs 0 and 1.
  // Invalid encoding the root arc is limited to 0, 1, and 2.
  if ((list[0] < 0) || (list[0] > 2) || (list[0] <= 1 && list[1] > 39)) {
    return {};
  }
  
  std::vector<std::uint8_t> result = encode_octet_as_septet(list[0] * 40 + list[1]);
  
  for(std::size_t i = 2; i < list.size(); ++i) {
    std::vector<std::uint8_t> encoded = encode_octet_as_septet(list[i]);
    result.insert(result.end(), encoded.begin(), encoded.end());
  }
  
  result.insert(result.begin(), result.size());
  result.insert(result.begin(), 0x06);
  return result;
}

std::string oid_to_absolute_oid(const std::vector<std::uint8_t>& oid) {
  // Invalid BER encoding
  if (oid.size() < 2) {
    return std::string();
  }
  
  // Drop first 2 octets as it isn't needed for the calculation
  std::uint32_t X = static_cast<std::uint32_t>(oid[2]) / 40;
  std::uint32_t Y = static_cast<std::uint32_t>(oid[2]) % 40;
  std::uint32_t sub = 0;
  
  std::string dot_notation;
  
  if (X > 2) {
    X = 2;
    
    dot_notation = std::to_string(X);
    if ((static_cast<std::uint32_t>(oid[2]) & 0x80) != 0x00) {
      sub = 80;
    } else {
      dot_notation += "." + std::to_string(Y + ((X - 2) * 40));
    }
  } else {
    dot_notation = std::to_string(X) + "." + std::to_string(Y);
  }
  
  // Drop first 2 octets as it isn't needed for the calculation
  // Start at the next octet
  std::uint64_t value = 0;
  for (std::size_t i = (sub ? 2 : 3); i < oid.size(); ++i) {
    value = (value << 7) | (static_cast<std::uint64_t>(oid[i]) & 0x7F);
    if ((static_cast<std::uint64_t>(oid[i]) & 0x80) != 0x80) {
      dot_notation += "." + std::to_string(value - sub);
      sub = 0;
      value = 0;
    }
  }
  return dot_notation;
}

std::string absolute_oid_to_oid_string(const std::string& oid) {
  std::vector<std::uint8_t> ber_encoding = absolute_oid_to_oid(oid);
  return std::string(ber_encoding.begin(), ber_encoding.end());
}

std::string oid_to_absolute_oid_string(const std::string& oid) {
  auto ber_decoding = std::vector<std::uint8_t>(oid.begin(), oid.end());
  return oid_to_absolute_oid(ber_decoding);
}

std::vector<net::der::Input> supported_extension_oids() {
  return {
    net::SubjectKeyIdentifierOid(),
    net::KeyUsageOid(),
    net::SubjectAltNameOid(),
    net::BasicConstraintsOid(),
    net::NameConstraintsOid(),
    net::CertificatePoliciesOid(),
    net::AuthorityKeyIdentifierOid(),
    net::PolicyConstraintsOid(),
    net::ExtKeyUsageOid(),
    net::AuthorityInfoAccessOid(),
    net::AdCaIssuersOid(),
    net::AdOcspOid(),
    net::CrlDistributionPointsOid()
  };
}

//bool ExtractEmbeddedSCTList(const CRYPTO_BUFFER* cert, std::string* sct_list) {
//  // The wire form of the OID 1.3.6.1.4.1.11129.2.4.2. See Section 3.3 of
//  // RFC6962.
//  const uint8_t kEmbeddedSCTOid[] = {0x2B, 0x06, 0x01, 0x04, 0x01,
//                                     0xD6, 0x79, 0x02, 0x04, 0x02};
//
//  CBS cert_cbs;
//  CBS_init(&cert_cbs, CRYPTO_BUFFER_data(cert), CRYPTO_BUFFER_len(cert));
//  CBS cert_body, tbs_cert, extensions_wrap, extensions;
//  if (!CBS_get_asn1(&cert_cbs, &cert_body, CBS_ASN1_SEQUENCE) ||
//      CBS_len(&cert_cbs) != 0 ||
//      !CBS_get_asn1(&cert_body, &tbs_cert, CBS_ASN1_SEQUENCE) ||
//      !SkipTBSCertificateToExtensions(&tbs_cert) ||
//      // Extract the extensions list.
//      !CBS_get_asn1(&tbs_cert, &extensions_wrap,
//                    CBS_ASN1_CONTEXT_SPECIFIC | CBS_ASN1_CONSTRUCTED | 3) ||
//      !CBS_get_asn1(&extensions_wrap, &extensions, CBS_ASN1_SEQUENCE) ||
//      CBS_len(&extensions_wrap) != 0 || CBS_len(&tbs_cert) != 0) {
//    return false;
//  }
//
//  return ParseSCTListFromExtensions(extensions, kEmbeddedSCTOid,
//                                    sizeof(kEmbeddedSCTOid), sct_list);
//}

bool ExtractEmbeddedSCT(const CRYPTO_BUFFER* cert,
                        std::vector<scoped_refptr<net::ct::SignedCertificateTimestamp>>& scts) {
  std::string sct_list;
  if (!net::ct::ExtractEmbeddedSCTList(cert, &sct_list)) {
    return false;
  }

  std::vector<base::StringPiece> parsed_scts;
  if (!net::ct::DecodeSCTList(sct_list, &parsed_scts)) {
    return false;
  }
  
  if (parsed_scts.empty()) {
    return false;
  }
  
  bool result = true;
  for (auto&& parsed_sct : parsed_scts) {
    scoped_refptr<net::ct::SignedCertificateTimestamp> sct(
          new net::ct::SignedCertificateTimestamp());
    result = net::ct::DecodeSignedCertificateTimestamp(&parsed_sct, &sct) && result;
    scts.emplace_back(sct);
  }
  return result;
}

bool ParseAlgorithmIdentifier(const net::der::Input& input,
                              net::der::Input* algorithm_oid,
                              net::der::Input* parameters) {
  net::der::Parser parser(input);

  net::der::Parser algorithm_identifier_parser;
  if (!parser.ReadSequence(&algorithm_identifier_parser)) {
    return false;
  }

  if (parser.HasMore()) {
    return false;
  }

  if (!algorithm_identifier_parser.ReadTag(net::der::kOid, algorithm_oid)) {
    return false;
  }

  *parameters = net::der::Input();
  if (algorithm_identifier_parser.HasMore() &&
      !algorithm_identifier_parser.ReadRawTLV(parameters)) {
    return false;
  }
  return !algorithm_identifier_parser.HasMore();
}

bool ParseAlgorithmSequence(const net::der::Input& input,
                            net::der::Input* algorithm_oid,
                            net::der::Input* parameters) {
  net::der::Parser parser(input);

  // Extract object identifier field
  if (!parser.ReadTag(net::der::kOid, algorithm_oid)) {
    return false;
  }
  
  if (!parser.HasMore()) {
    return false;
  }
  
  // Extract the parameters field.
  *parameters = net::der::Input();
  if (parser.HasMore() &&
      !parser.ReadRawTLV(parameters)) {
    return false;
  }
  return !parser.HasMore();
}

bool ParseSubjectPublicKeyInfo(const net::der::Input& input,
                               net::der::Input* algorithm_sequence,
                               net::der::Input* spk) {
  // From RFC 5280, Section 4.1
  //   SubjectPublicKeyInfo  ::=  SEQUENCE  {
  //     algorithm            AlgorithmIdentifier,
  //     subjectPublicKey     BIT STRING  }
  //
  //   AlgorithmIdentifier  ::=  SEQUENCE  {
  //     algorithm               OBJECT IDENTIFIER,
  //     parameters              ANY DEFINED BY algorithm OPTIONAL  }

  net::der::Parser parser(input);
  net::der::Parser spki_parser;
  if (!parser.ReadSequence(&spki_parser)) {
    return false;
  }

  // Extract algorithm field.
  // ReadSequenceTLV then maybe ParseAlgorithmIdentifier instead.
  if (!spki_parser.ReadTag(net::der::kSequence, algorithm_sequence)) {
    return false;
  }
  
  if (!spki_parser.HasMore()) {
    return false;
  }
  
  // Extract the subjectPublicKey field.
  if (!spki_parser.ReadTag(net::der::kBitString, spk)) {
    return false;
  }
  return true;
}

std::string signature_algorithm_digest_to_name(const net::SignatureAlgorithm& signature_algorithm) {
  std::unordered_map<net::DigestAlgorithm, std::string> mapping = {
    {net::DigestAlgorithm::Md2, "MD2"},
    {net::DigestAlgorithm::Md4, "MD4"},
    {net::DigestAlgorithm::Md5, "MD5"},
    {net::DigestAlgorithm::Sha1, "SHA-1"},
    {net::DigestAlgorithm::Sha256, "SHA-256"},
    {net::DigestAlgorithm::Sha384, "SHA-384"},
    {net::DigestAlgorithm::Sha512, "SHA-512"}
  };
  
  auto it = mapping.find(signature_algorithm.digest());
  if (it != mapping.end()) {
    return it->second;
  }
  return std::string();
}

std::string signature_algorithm_id_to_name(const net::SignatureAlgorithm& signature_algorithm) {
  std::unordered_map<net::SignatureAlgorithmId, std::string> mapping = {
    {net::SignatureAlgorithmId::RsaPkcs1, "RSA"},
    {net::SignatureAlgorithmId::RsaPss, "RSA-PSS"},
    {net::SignatureAlgorithmId::Ecdsa, "ECDSA"},
    {net::SignatureAlgorithmId::Dsa, "DSA"}
  };
  
  auto it = mapping.find(signature_algorithm.algorithm());
  if (it != mapping.end()) {
    return it->second;
  }
  return std::string();
}

base::Time generalized_time_to_time(const net::der::GeneralizedTime& generalized_time) {
  base::Time time;
  net::der::GeneralizedTimeToTime(generalized_time, &time);
  return time;
}
}  // namspace x509_utils

namespace brave {
template<typename T>
struct is_objective_c_type {
private:
  template<class U> struct remove_pointer                    {typedef U type;};
  template<class U> struct remove_pointer<U*>                {typedef typename remove_pointer<U>::type type;};
  template<class U> struct remove_pointer<U* const>          {typedef typename remove_pointer<U>::type type;};
  template<class U> struct remove_pointer<U* volatile>       {typedef typename remove_pointer<U>::type type;};
  template<class U> struct remove_pointer<U* const volatile> {typedef typename remove_pointer<U>::type type;};
  
public:
  static const bool value = std::is_base_of<NSObject, typename remove_pointer<T>::type>::value ||
                            std::is_convertible<typename remove_pointer<T>::type, NSObject*>::value;
};

NSString* string_to_ns(const std::string& string) {
  return [NSString stringWithUTF8String:string.c_str()];
}

NSDate* date_to_ns(const base::Time& date) {
  return [NSDate dateWithTimeIntervalSince1970:date.ToDoubleT()];
}

template<typename T>
auto vector_to_ns(const std::vector<T>& vector) -> typename std::enable_if<is_objective_c_type<T>::value, NSArray*>::type {
  NSMutableArray* array = [[NSMutableArray alloc] init];
  for (const T& value : vector) {
    [array addObject:value];
  }
  return array;
}

template<typename T>
auto vector_to_ns(const std::vector<T>& vector) -> typename std::enable_if<std::is_fundamental<T>::value, NSArray*>::type {
  NSMutableArray* array = [[NSMutableArray alloc] init];
  for (const T& value : vector) {
    [array addObject:@(value)];
  }
  return array;
}

template<typename T>
auto vector_to_ns(const std::vector<T>& vector) -> typename std::enable_if<std::is_same<T, std::string>::value, NSArray*>::type {
  NSMutableArray* array = [[NSMutableArray alloc] init];
  for (const T& value : vector) {
    [array addObject:[NSString stringWithUTF8String:value.c_str()]];
  }
  return array;
}
}  // namespace brave



@implementation BraveCertificateRDNSequenceModel
- (instancetype)initWithBERName:(const net::der::Input&)berName uniqueId:(const net::der::BitString&)uniqueId {
  if ((self = [super init])) {
    net::CertPrincipal rdns;  // relative_distinquished_name_sequence;
    auto string_handling = net::CertPrincipal::PrintableStringHandling::kDefault;
    if (!rdns.ParseDistinguishedName(berName.UnsafeData(),
                                     berName.Length(),
                                     string_handling)) {
      _stateOrProvince = [[NSString alloc] init];
      _locality = [[NSString alloc] init];
      _organization = [[NSArray alloc] init];
      _organizationalUnit = [[NSArray alloc] init];
      _commonName = [[NSString alloc] init];
      _streetAddress = [[NSArray alloc] init];
      _domainComponent = [[NSArray alloc] init];
      _userId = [[NSString alloc] init];
      _countryOrRegion = [[NSString alloc] init];
      return self;
    }
    
    _countryOrRegion = brave::string_to_ns(rdns.country_name);
    _stateOrProvince = brave::string_to_ns(rdns.state_or_province_name);
    _locality = brave::string_to_ns(rdns.locality_name);
    _organization = brave::vector_to_ns(rdns.organization_names);
    _organizationalUnit = brave::vector_to_ns(rdns.organization_unit_names);
    _commonName = brave::string_to_ns(rdns.common_name);
    _streetAddress = brave::vector_to_ns(rdns.street_addresses);
    _domainComponent = brave::vector_to_ns(rdns.domain_components);
    _userId = brave::string_to_ns(uniqueId.bytes().AsString());
  }
  return self;
}
@end

@implementation BraveCertificateSignature
- (instancetype)initWithCertificate:(const net::ParsedCertificate*)certificate {
  if ((self = [super init])) {
    _algorithm = [[NSString alloc] init];
    _digest = [[NSString alloc] init];
    _objectIdentifier = [[NSString alloc] init];
    _signatureHexEncoded = [[NSString alloc] init];
    _parameters = [[NSString alloc] init];
    _bytesSize = 0;
    
//    net::CertErrors errors;
//    net::SignatureAlgorithm signature_algorithm = net::SignatureAlgorithm::Create(
//        certificate->signature_algorithm(),
//        &errors);
//
//    if (!signature_algorithm) {
//      NSLog(@"Error: %s\n", errors.ToDebugString().c_str());
//      return self;
//    }

    _digest = brave::string_to_ns(
                  x509_utils::signature_algorithm_digest_to_name(certificate->signature_algorithm()));
    _algorithm = brave::string_to_ns(
                     x509_utils::signature_algorithm_id_to_name(certificate->signature_algorithm()));
    
    net::der::Input signature_oid;
    net::der::Input signature_params;
    if (x509_utils::ParseAlgorithmIdentifier(certificate->signature_algorithm_tlv(),
                                             &signature_oid, &signature_params)) {
      _objectIdentifier = brave::string_to_ns(
                              x509_utils::oid_to_absolute_oid_string(signature_oid.AsString()));
      _parameters = brave::string_to_ns(x509_utils::hex_string_from_string(signature_params.AsString()));
    }
    
    _signatureHexEncoded = brave::string_to_ns(
                               x509_utils::hex_string_from_string(
                                   certificate->signature_value().bytes().AsString()));
    _bytesSize = certificate->signature_value().bytes().Length();
  }
  return self;
}
@end

@implementation BraveCertificatePublicKeyInfo
// Chromium's GetSubjectPublicKeyBytes is NOT enough
// It only gives the Raw TLV BIT_STRING of the SubjectPublicKeyInfo
- (instancetype)initWithCertificate:(net::ParsedCertificate*)certificate withKey:(SecKeyRef)key {
  if ((self = [super init])) {
    _type = BravePublicKeyType_UNKNOWN;
    _keyUsage = BravePublicKeyUsage_INVALID;
    _algorithm = [[NSString alloc] init];
    _objectIdentifier = [[NSString alloc] init];
    _curveName = [[NSString alloc] init];
    _nistCurveName = [[NSString alloc] init];
    _parameters = [[NSString alloc] init];
    _keyHexEncoded = [[NSString alloc] init];
    _keyBytesSize = 0;
    _exponent = 0;
    _keySizeInBits = 0;
    
    net::der::Input algorithm_tlv;
    net::der::Input spk;
    if (x509_utils::ParseSubjectPublicKeyInfo(certificate->tbs().spki_tlv, &algorithm_tlv, &spk)) {
      net::der::Input algorithm_oid;
      net::der::Input parameters;
      
      if (x509_utils::ParseAlgorithmSequence(algorithm_tlv, &algorithm_oid, &parameters)) {
        _objectIdentifier = brave::string_to_ns(
                                x509_utils::oid_to_absolute_oid_string(algorithm_oid.AsString()));
        _parameters = brave::string_to_ns(x509_utils::hex_string_from_string(parameters.AsString()));
      }
      
      /*// SPK has the unused bit count. Remove it.
      if (!base::StartsWith(spk.AsStringPiece(), "\0")) {
        return false;
      }
      spk.AsStringPiece().remove_prefix(1);
      spk = der::Input(spk.AsStringPiece());*/
    }
    
    _keyBytesSize = SecKeyGetBlockSize(key);
    NSData* external_representation = (__bridge_transfer NSData*)SecKeyCopyExternalRepresentation(key, nil);
    _keyHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_bytes(static_cast<const std::uint8_t*>([external_representation bytes]),
                                                                           [external_representation length]));

    NSDictionary* attributes = (__bridge_transfer NSDictionary*)SecKeyCopyAttributes(key);
    if (attributes) {
      _keySizeInBits = [attributes[(NSString*)kSecAttrKeySizeInBits] integerValue];
      //_effectiveSize = [(__bridge NSNumber*)attrs[kSecAttrEffectiveKeySize] integerValue];
      
      if ([attributes[(NSString*)kSecAttrCanEncrypt] boolValue]) {
        _keyUsage |= BravePublicKeyUsage_ENCRYPT;
      }
      
      if ([attributes[(NSString*)kSecAttrCanSign] boolValue]) {
        _keyUsage |= BravePublicKeyUsage_SIGN;
      }
      
      if ([attributes[(NSString*)kSecAttrCanVerify] boolValue]) {
        _keyUsage |= BravePublicKeyUsage_VERIFY;
      }
      
      if ([attributes[(NSString*)kSecAttrCanWrap] boolValue]) {
        _keyUsage |= BravePublicKeyUsage_WRAP;
      }
      
      if ([attributes[(NSString*)kSecAttrCanDerive] boolValue]) {
        _keyUsage |= BravePublicKeyUsage_DERIVE;
      }
      
      BravePublicKeyUsage any_usage = 0;
      any_usage &= BravePublicKeyUsage_ENCRYPT;
      any_usage &= BravePublicKeyUsage_SIGN;
      any_usage &= BravePublicKeyUsage_VERIFY;
      any_usage &= BravePublicKeyUsage_WRAP;
      any_usage &= BravePublicKeyUsage_DERIVE;
      
      if (_keyUsage == any_usage) {
        _keyUsage = BravePublicKeyUsage_ANY;
      }
      
      if ([attributes[(NSString*)kSecAttrType] isEqualToString:(NSString*)kSecAttrKeyTypeRSA]) {
        _type = BravePublicKeyType_RSA;
        _algorithm = @"RSA";
      }
      
      if ([attributes[(NSString*)kSecAttrType] isEqualToString:(NSString*)kSecAttrKeyTypeEC]) {
        _type = BravePublicKeyType_EC;
        _algorithm = @"Elliptic Curve";
      }
      
      if ([attributes[(NSString*)kSecAttrType] isEqualToString:(NSString*)kSecAttrKeyTypeECSECPrimeRandom]) {
        _type = BravePublicKeyType_EC;
        _algorithm = @"Elliptic Curve (Prime Random)";
      }
    }
    
    // Parse SPKI
    net::CertErrors key_errors;
    bssl::UniquePtr<EVP_PKEY> pkey;
    if (!net::ParsePublicKey(certificate->tbs().spki_tlv, &pkey)) {
      key_errors.AddError(net::cert_errors::kFailedParsingSpki);
      NSLog(@"Error: %s\n", key_errors.ToDebugString().c_str());
      return self;
    }
    
    if (!pkey) {
      NSLog(@"Error: %s\n", key_errors.ToDebugString().c_str());
      return self;
    }
    
    // TODO Parse Key further for EC Curve Name and RSA Exponent
    // TODO Parse Key further for Raw Key without ASN1 DER Header/PKCS
  }
  return self;
}
@end

@implementation BraveCertificateFingerprint
- (instancetype)initWithCertificate:(CFDataRef)cert_data withType:(BraveFingerprintType)type {
  if ((self = [super init])) {
    //OpenSSL_add_all_digests()
    _type = type;
    
    switch (type) {
      case BraveFingerprintType_SHA1: {
        std::string data = std::string(SHA_DIGEST_LENGTH, '\0');

        SHA_CTX ctx;
        SHA1_Init(&ctx);
        SHA1_Update(&ctx, reinterpret_cast<const unsigned char*>(CFDataGetBytePtr(cert_data)), CFDataGetLength(cert_data));
        SHA1_Final(reinterpret_cast<unsigned char*>(&data[0]), &ctx);
        
        _fingerprintHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_string(data));
      }
        break;
        
      case BraveFingerprintType_SHA256:{
        std::string data = std::string(SHA256_DIGEST_LENGTH, '\0');

        SHA256_CTX ctx;
        SHA256_Init(&ctx);
        SHA256_Update(&ctx, reinterpret_cast<const unsigned char*>(CFDataGetBytePtr(cert_data)), CFDataGetLength(cert_data));
        SHA256_Final(reinterpret_cast<unsigned char*>(&data[0]), &ctx);
        
        _fingerprintHexEncoded = brave::string_to_ns(x509_utils::hex_string_from_string(data));
      }
        break;
    }
  }
  return self;
}
@end

@interface BraveCertificateModel()
{
  base::ScopedCFTypeRef<CFDataRef> cert_data_;
  scoped_refptr<net::ParsedCertificate> extended_cert_;
  base::ScopedCFTypeRef<SecKeyRef> public_key_;
}
@end

@implementation BraveCertificateModel
- (nullable instancetype)initWithCertificate:(nonnull SecCertificateRef)certificate {
  if ((self = [super init])) {
    cert_data_ = base::ScopedCFTypeRef<CFDataRef>(SecCertificateCopyData(certificate));
    if (!cert_data_) {
      return nullptr;
    }
    
    public_key_ = base::ScopedCFTypeRef<SecKeyRef>(SecCertificateCopyKey(certificate));
    
    bssl::UniquePtr<CRYPTO_BUFFER> cert_buffer(
       net::X509Certificate::CreateCertBufferFromBytes(
          reinterpret_cast<const char*>(CFDataGetBytePtr(cert_data_)),
                                        CFDataGetLength(cert_data_)));
    
    if (!cert_buffer) {
      return nullptr;
    }
    
    net::CertErrors errors;
    extended_cert_ = scoped_refptr<net::ParsedCertificate>(
                         net::ParsedCertificate::Create(
                            std::move(cert_buffer),
                            net::x509_util::DefaultParseCertificateOptions() /* {} */,
                            &errors)
                         );
    
    if (!extended_cert_) {
      NSLog(@"Error: %s\n", errors.ToDebugString().c_str());
      return nullptr;
    }
    
    [self parseCertificate];
    
    cert_data_.reset();
    extended_cert_.reset();
    public_key_.reset();
  }
  return self;
}

- (void)dealloc {
  extended_cert_.reset();
}

- (void)parseCertificate {
  _isRootCertificate = [self is_root_certificate];
  _isCertificateAuthority = [self is_certificate_authority];
  _isSelfSigned = [self is_self_signed];
  _isSelfIssued = [self is_self_issued];
  _subjectName = [[BraveCertificateRDNSequenceModel alloc] initWithBERName:extended_cert_->tbs().subject_tlv
                                                                  uniqueId:extended_cert_->tbs().subject_unique_id];
  _issuerName = [[BraveCertificateRDNSequenceModel alloc] initWithBERName:extended_cert_->tbs().issuer_tlv
                                                                 uniqueId:extended_cert_->tbs().issuer_unique_id];
  _serialNumber = brave::string_to_ns(x509_utils::hex_string_from_string(extended_cert_->tbs().serial_number.AsString()));  // base::HexEncode(data, length)
  _version = static_cast<std::int32_t>(extended_cert_->tbs().version) + 1;  // version + 1
  _signature = [[BraveCertificateSignature alloc] initWithCertificate:extended_cert_.get()];
  _notValidBefore = brave::date_to_ns(x509_utils::generalized_time_to_time(extended_cert_->tbs().validity_not_before));
  _notValidAfter = brave::date_to_ns(x509_utils::generalized_time_to_time(extended_cert_->tbs().validity_not_after));
  _publicKeyInfo = [[BraveCertificatePublicKeyInfo alloc] initWithCertificate:extended_cert_.get() withKey:public_key_.get()];
//  _extensions = nullptr;
  _sha1Fingerprint = [[BraveCertificateFingerprint alloc] initWithCertificate:cert_data_.get()
                                                                     withType:BraveFingerprintType_SHA1];
  _sha256Fingerprint = [[BraveCertificateFingerprint alloc] initWithCertificate:cert_data_.get()
                                                                       withType:BraveFingerprintType_SHA256];
  
  [self parseExtensions];
}

- (bool)is_root_certificate {
  net::CertErrors errors;
  std::string normalized_subject;
  net::der::Input subject_value = extended_cert_->normalized_subject();
  if (!net::NormalizeName(subject_value, &normalized_subject, &errors)) {
    return false;
  }
  
  std::string normalized_issuer;
  net::der::Input issuer_value = extended_cert_->normalized_issuer();
  if (!net::NormalizeName(issuer_value, &normalized_issuer, &errors)) {
    return false;
  }
  
  return normalized_subject == normalized_issuer;
}

- (bool)is_certificate_authority {
  //return X509_check_ca(x509_cert_);
  return false;
}

- (bool)is_self_signed {
  net::CertErrors errors;
  std::string normalized_subject;
  net::der::Input subject_value = extended_cert_->normalized_subject();
  if (!net::NormalizeName(subject_value, &normalized_subject, &errors)) {
    return false;
  }
  
  std::string normalized_issuer;
  net::der::Input issuer_value = extended_cert_->normalized_issuer();
  if (!net::NormalizeName(issuer_value, &normalized_issuer, &errors)) {
    return false;
  }
  
  if (normalized_subject != normalized_issuer) {
    return false;
  }
  
  return net::VerifySignedData(extended_cert_->signature_algorithm(),
                               extended_cert_->tbs_certificate_tlv(),
                               extended_cert_->signature_value(),
                               extended_cert_->tbs().spki_tlv);
}

- (bool)is_self_issued {
  //return X509_get_extension_flags(x509_cert_) & EXFLAG_SI;
  return false;
}

- (void)parseExtensions {
  
}
@end

