/* Copyright (c) 2021 The Brave Authors. All rights reserved.
 * This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this file,
 * You can obtain one at http://mozilla.org/MPL/2.0/. */


#ifndef BRAVE_IOS_BROWSER_API_NETWORKING_BRAVE_CERTIFICATE_MODEL_H_
#define BRAVE_IOS_BROWSER_API_NETWORKING_BRAVE_CERTIFICATE_MODEL_H_

#import <Foundation/Foundation.h>
#import <Security/Security.h>

#if defined(BRAVE_CORE)
#include "brave/ios/browser/api/networking/common/brave_certificate_enums.h"
#else
#include <BraveRewards/brave_certificate_enums.h>
#endif

NS_ASSUME_NONNULL_BEGIN

@class BraveCertificateSubjectModel;
@class BraveCertificateIssuerName;
@class BraveCertificateSignature;
@class BraveCertificatePublicKeyInfo;
@class BraveCertificateFingerprint;
//@class BraveCertificateExtensionModel;


OBJC_EXPORT
@interface BraveCertificateRDNSequenceModel: NSObject
@property(nonatomic, strong, readonly) NSString* countryOrRegion;
@property(nonatomic, strong, readonly) NSString* stateOrProvince;
@property(nonatomic, strong, readonly) NSString* locality;
@property(nonatomic, strong, readonly) NSArray<NSString*>* organization;
@property(nonatomic, strong, readonly) NSArray<NSString*>* organizationalUnit;
@property(nonatomic, strong, readonly) NSString* commonName;
@property(nonatomic, strong, readonly) NSArray<NSString*>* streetAddress;
@property(nonatomic, strong, readonly) NSArray<NSString*>* domainComponent;
@property(nonatomic, strong, readonly) NSString* userId;
@end

OBJC_EXPORT
@interface BraveCertificateSignature: NSObject
@property(nonatomic, strong, readonly) NSString* algorithm;
@property(nonatomic, strong, readonly) NSString* digest;
@property(nonatomic, strong, readonly) NSString* objectIdentifier;
@property(nonatomic, strong, readonly) NSString* signatureHexEncoded;
@property(nonatomic, strong, readonly) NSString* parameters;
@property(nonatomic, assign, readonly) NSUInteger bytesSize;
@end

OBJC_EXPORT
@interface BraveCertificatePublicKeyInfo: NSObject
@property(nonatomic, assign, readonly) BravePublicKeyType type;
@property(nonatomic, strong, readonly) NSString* algorithm;
@property(nonatomic, strong, readonly) NSString* objectIdentifier;
@property(nonatomic, strong, readonly) NSString* curveName;
@property(nonatomic, strong, readonly) NSString* nistCurveName;
@property(nonatomic, strong, readonly) NSString* parameters;
@property(nonatomic, strong, readonly) NSString* keyHexEncoded;
@property(nonatomic, assign, readonly) NSUInteger keyBytesSize;
@property(nonatomic, assign, readonly) NSUInteger exponent;
@property(nonatomic, assign, readonly) NSUInteger keySizeInBits;
@property(nonatomic, assign, readonly) BraveKeyUsage keyUsage;
@end

OBJC_EXPORT
@interface BraveCertificateFingerprint: NSObject
@property(nonatomic, assign, readonly) BraveFingerprintType type;
@property(nonatomic, strong, readonly) NSString* fingerprintHexEncoded;
@end

OBJC_EXPORT
@interface BraveCertificateModel: NSObject
@property(nonatomic, assign, readonly) bool isRootCertificate;
@property(nonatomic, assign, readonly) bool isCertificateAuthority;
@property(nonatomic, assign, readonly) bool isSelfSigned;
@property(nonatomic, assign, readonly) bool isSelfIssued;
@property(nonatomic, strong, readonly) BraveCertificateRDNSequenceModel* subjectName;
@property(nonatomic, strong, readonly) BraveCertificateRDNSequenceModel* issuerName;
@property(nonatomic, strong, readonly) NSString* serialNumber;
@property(nonatomic, assign, readonly) NSUInteger version;
@property(nonatomic, strong, readonly) BraveCertificateSignature* signature;
@property(nonatomic, strong, readonly) NSDate* notValidBefore;
@property(nonatomic, strong, readonly) NSDate* notValidAfter;
@property(nonatomic, strong, readonly) BraveCertificatePublicKeyInfo* publicKeyInfo;
//@property(nonatomic, strong, readonly) NSArray<BraveCertificateExtensionModel*>* extensions;
@property(nonatomic, strong, readonly) BraveCertificateFingerprint* sha1Fingerprint;
@property(nonatomic, strong, readonly) BraveCertificateFingerprint* sha256Fingerprint;

- (nullable instancetype)initWithCertificate:(nonnull SecCertificateRef)certificate;
@end

NS_ASSUME_NONNULL_END

#endif  // BRAVE_IOS_BROWSER_API_NETWORKING_BRAVE_CERTIFICATE_MODEL_H_
