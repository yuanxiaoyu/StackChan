/*
 * SPDX-FileCopyrightText: 2026 M5Stack Technology CO LTD
 *
 * SPDX-License-Identifier: MIT
 */
#include "secret_logic.h"
#include "esp_mac.h"
#include <sdkconfig.h>
#include <psa/crypto.h>
#include <esp_log.h>
#include <cstdio>
#include <cstring>
#include <string>

namespace secret_logic {

static constexpr const char *kTag = "SecretLogic";

__attribute__((weak)) std::string get_server_url()
{
#ifdef CONFIG_STACKCHAN_SERVER_URL
    return CONFIG_STACKCHAN_SERVER_URL;
#else
    return "http://localhost:3000";
#endif
}

__attribute__((weak)) std::string generate_auth_token()
{
    return "hi-stack-chan";
}

__attribute__((weak)) std::string generate_handshake_token(std::string_view data)
{
    // StackChan World decrypts this value with its device-link RSA private key.
    // Keep the public key in firmware only; the corresponding private key never
    // belongs in the device image.
    static constexpr uint8_t kStackChanBluePublicKey[] = {
        0x30, 0x82, 0x01, 0x22, 0x30, 0x0d, 0x06, 0x09, 0x2a, 0x86, 0x48, 0x86, 0xf7, 0x0d, 0x01, 0x01,
        0x01, 0x05, 0x00, 0x03, 0x82, 0x01, 0x0f, 0x00, 0x30, 0x82, 0x01, 0x0a, 0x02, 0x82, 0x01, 0x01,
        0x00, 0xcc, 0x40, 0x3d, 0x07, 0x3b, 0xa5, 0xa6, 0x77, 0x0c, 0xbe, 0x95, 0x29, 0xd9, 0xa0, 0x2c,
        0x4d, 0x14, 0xa2, 0x07, 0x99, 0xef, 0x17, 0x8a, 0x10, 0xa5, 0xb7, 0xd8, 0x8c, 0x67, 0x86, 0x22,
        0x70, 0xe4, 0x6b, 0x56, 0x73, 0x17, 0x2d, 0x32, 0x27, 0x89, 0xa3, 0xc6, 0x53, 0xc5, 0x27, 0xd0,
        0x1c, 0x65, 0x42, 0x09, 0x0f, 0x6b, 0xd6, 0xc5, 0x45, 0x0a, 0x40, 0xe7, 0xfd, 0x7d, 0xe5, 0x3c,
        0x45, 0x11, 0x09, 0x08, 0x72, 0x1e, 0xda, 0xb1, 0x21, 0xc7, 0x52, 0xf3, 0xbc, 0x86, 0xed, 0xd8,
        0x56, 0x83, 0x82, 0xea, 0x76, 0x41, 0x44, 0x90, 0xb2, 0x30, 0x17, 0xf8, 0xc2, 0x9e, 0x4b, 0xad,
        0x76, 0xd4, 0x04, 0x9c, 0xac, 0x72, 0x53, 0xbf, 0xd3, 0x3b, 0xb5, 0x69, 0xd5, 0x63, 0x0b, 0x33,
        0xea, 0x91, 0xe4, 0xef, 0xfe, 0xaa, 0x27, 0x73, 0x0f, 0xd4, 0x4e, 0xc9, 0x1d, 0xba, 0x9c, 0xf3,
        0x2f, 0x77, 0x54, 0x14, 0x6a, 0x94, 0xe1, 0x4d, 0x81, 0x41, 0x97, 0x9a, 0xca, 0x4f, 0xe4, 0xc1,
        0xee, 0x85, 0x2c, 0x93, 0x8f, 0xc5, 0xab, 0x29, 0xec, 0x3e, 0x8d, 0xf3, 0x32, 0xbb, 0x15, 0x41,
        0x98, 0x85, 0xa9, 0x18, 0xde, 0x70, 0x78, 0xcd, 0xe0, 0x7b, 0x05, 0x4a, 0x26, 0xe7, 0xe2, 0x0b,
        0x68, 0x1e, 0x17, 0x8f, 0x83, 0xf6, 0x01, 0xc7, 0x8a, 0xf8, 0xeb, 0x90, 0x6f, 0xf8, 0x93, 0x96,
        0x0c, 0xe6, 0xed, 0xed, 0x15, 0x0e, 0xcd, 0xfb, 0x78, 0x35, 0x73, 0x1e, 0xeb, 0x0b, 0x53, 0x9a,
        0xf5, 0x49, 0xb2, 0x77, 0x17, 0x74, 0x50, 0xc7, 0x42, 0x55, 0xa7, 0x14, 0xb4, 0xfa, 0x68, 0xdc,
        0x7b, 0x26, 0x2b, 0x97, 0xd3, 0xc0, 0xf2, 0x7f, 0x9d, 0x2e, 0xa7, 0xe4, 0x49, 0x5c, 0xd7, 0xca,
        0xbe, 0xd3, 0xcd, 0x72, 0xa4, 0x4d, 0x58, 0xdd, 0xa6, 0x3c, 0x30, 0xc8, 0xe2, 0xd3, 0xb5, 0x49,
        0x8d, 0x02, 0x03, 0x01, 0x00, 0x01};

    uint8_t mac[6] = {};
    if (esp_read_mac(mac, ESP_MAC_EFUSE_FACTORY) != ESP_OK) {
        return {};
    }

    char plain[128] = {};
    const int plain_len = std::snprintf(
        plain, sizeof(plain), "%02X%02X%02X%02X%02X%02X|%.*s|stackchan",
        mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], static_cast<int>(data.size()), data.data());
    if (plain_len <= 0 || static_cast<size_t>(plain_len) >= sizeof(plain)) {
        return {};
    }

    std::string result;
    static constexpr char kBase64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    psa_status_t status = psa_crypto_init();
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_key_id_t key_id = 0;
    psa_set_key_type(&attributes, PSA_KEY_TYPE_RSA_PUBLIC_KEY);
    psa_set_key_bits(&attributes, 2048);
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attributes, PSA_ALG_RSA_OAEP(PSA_ALG_SHA_256));
    if (status == PSA_SUCCESS) {
        // PSA's standard RSA public-key import format is the PKCS#1
        // RSAPublicKey sequence. The embedded key is an X.509
        // SubjectPublicKeyInfo wrapper; skip its 24-byte algorithm and
        // BIT STRING header before importing the inner sequence.
        constexpr size_t kSubjectPublicKeyInfoHeaderLen = 24;
        status = psa_import_key(&attributes, kStackChanBluePublicKey + kSubjectPublicKeyInfoHeaderLen,
                                sizeof(kStackChanBluePublicKey) - kSubjectPublicKeyInfoHeaderLen, &key_id);
    }
    if (status != PSA_SUCCESS) {
        ESP_LOGE(kTag, "BLE RSA key setup failed: status=%ld, der=%u", static_cast<long>(status),
                 static_cast<unsigned>(sizeof(kStackChanBluePublicKey) - 24));
    }
    uint8_t encrypted[256] = {};
    size_t encrypted_len = 0;
    if (status == PSA_SUCCESS) {
        status = psa_asymmetric_encrypt(key_id, PSA_ALG_RSA_OAEP(PSA_ALG_SHA_256),
                                        reinterpret_cast<const uint8_t *>(plain), plain_len, nullptr, 0,
                                        encrypted, sizeof(encrypted), &encrypted_len);
    }
    if (status != PSA_SUCCESS) {
        ESP_LOGE(kTag, "BLE RSA encryption failed: status=%ld, plain=%d", static_cast<long>(status), plain_len);
    }
    if (key_id != 0) psa_destroy_key(key_id);
    psa_reset_key_attributes(&attributes);
    if (status == PSA_SUCCESS) {
        result.reserve(((encrypted_len + 2) / 3) * 4);
        for (size_t i = 0; i < encrypted_len; i += 3) {
            const uint32_t value = (static_cast<uint32_t>(encrypted[i]) << 16) |
                                   (static_cast<uint32_t>(i + 1 < encrypted_len ? encrypted[i + 1] : 0) << 8) |
                                   static_cast<uint32_t>(i + 2 < encrypted_len ? encrypted[i + 2] : 0);
            result.push_back(kBase64[(value >> 18) & 0x3f]);
            result.push_back(kBase64[(value >> 12) & 0x3f]);
            result.push_back(i + 1 < encrypted_len ? kBase64[(value >> 6) & 0x3f] : '=');
            result.push_back(i + 2 < encrypted_len ? kBase64[value & 0x3f] : '=');
        }
    }
    return result;
}

}  // namespace secret_logic
