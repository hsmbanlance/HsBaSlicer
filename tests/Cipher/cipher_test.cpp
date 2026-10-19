#define BOOST_TEST_MODULE cipher_test
#include <boost/test/included/unit_test.hpp>

#include <string>
#include <vector>

#include "base/error.hpp"
#include "cipher/encoder.hpp"
#include "cipher/encrypt.hpp"
#include "cipher/hasher.hpp"

#include <openssl/bn.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>

using namespace HsBa::Slicer::Cipher;
using HsBa::Slicer::RuntimeError;

struct DisableCrt
{
    DisableCrt()
    {
#if defined(_MSC_VER) && defined(_DEBUG)
        _CrtSetDbgFlag(_CrtSetDbgFlag(_CRTDBG_REPORT_FLAG) & ~_CRTDBG_LEAK_CHECK_DF);
#endif  // defined(_MSC_VER) && defined(_DEBUG)
    }
};

namespace
{
std::pair<std::string, std::string> generate_rsa_pem_pair()
{
    EVP_PKEY* pkey = nullptr;
    RSA* rsa = nullptr;
    BIGNUM* bn = BN_new();
    BN_set_word(bn, RSA_F4);
    rsa = RSA_new();
    RSA_generate_key_ex(rsa, 2048, bn, nullptr);
    BN_free(bn);

    pkey = EVP_PKEY_new();
    EVP_PKEY_assign_RSA(pkey, rsa);  // pkey owns rsa now

    BIO* bio_priv = BIO_new(BIO_s_mem());
    BIO* bio_pub = BIO_new(BIO_s_mem());
    PEM_write_bio_PrivateKey(bio_priv, pkey, nullptr, nullptr, 0, nullptr, nullptr);
    PEM_write_bio_PUBKEY(bio_pub, pkey);

    char* priv_data = nullptr;
    long priv_len = BIO_get_mem_data(bio_priv, &priv_data);
    std::string priv_pem(priv_data, priv_len);

    char* pub_data = nullptr;
    long pub_len = BIO_get_mem_data(bio_pub, &pub_data);
    std::string pub_pem(pub_data, pub_len);

    BIO_free(bio_priv);
    BIO_free(bio_pub);
    EVP_PKEY_free(pkey);

    return {pub_pem, priv_pem};
}
}  // namespace

BOOST_AUTO_TEST_SUITE(cipher_tests)

BOOST_AUTO_TEST_CASE(encoder_roundtrip)
{
    [[maybe_unused]]
    static DisableCrt crt;
    std::vector<unsigned char> data = {1, 2, 3, 4, 5, 6, 7, 8, 9};
    auto b64 = Encoder::base64_encode(data);
    auto dec = Encoder::base64_decode(b64);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(data.begin(), data.end(), dec.begin(), dec.end());

    auto hex = Encoder::hex_encode(data);
    auto dec2 = Encoder::hex_decode(hex);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(data.begin(), data.end(), dec2.begin(), dec2.end());
}

BOOST_AUTO_TEST_CASE(aes_roundtrip)
{
    std::string pass = "testpass";
    std::vector<unsigned char> plain = {10, 20, 30, 40, 50, 60, 70, 80};

    auto c = Encrypt::aes256_ecb_encrypt(plain, pass);
    auto p = Encrypt::aes256_ecb_decrypt(c, pass);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p.begin(), p.end());

    std::vector<unsigned char> iv(16);
    for (int i = 0; i < 16; ++i)
        iv[i] = static_cast<unsigned char>(i + 1);
    auto c2 = Encrypt::aes256_cbc_encrypt_with_iv(plain, pass, iv);
    auto p2 = Encrypt::aes256_cbc_decrypt_with_iv(c2, pass, iv);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p2.begin(), p2.end());
}

BOOST_AUTO_TEST_CASE(des3_roundtrip)
{
    std::string pass = "3despass";
    std::vector<unsigned char> plain = {5, 4, 3, 2, 1, 9, 8, 7};

    auto c = Encrypt::des3_ecb_encrypt(plain, pass);
    auto p = Encrypt::des3_ecb_decrypt(c, pass);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p.begin(), p.end());

    std::vector<unsigned char> iv(8);
    for (int i = 0; i < 8; ++i)
        iv[i] = static_cast<unsigned char>(i + 1);
    auto c2 = Encrypt::des3_cbc_encrypt_with_iv(plain, pass, iv);
    auto p2 = Encrypt::des3_cbc_decrypt_with_iv(c2, pass, iv);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p2.begin(), p2.end());
}

BOOST_AUTO_TEST_CASE(rsa_roundtrip)
{
    auto pair = generate_rsa_pem_pair();
    std::string pub = pair.first;
    std::string priv = pair.second;

    std::vector<unsigned char> plain = {11, 22, 33, 44, 55};
    auto cipher = Encrypt::rsa_public_encrypt_pem(pub, plain);
    auto out = Encrypt::rsa_private_decrypt_pem(priv, cipher);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), out.begin(), out.end());
}

BOOST_AUTO_TEST_CASE(rsa_gen_and_use)
{
    // Use library function to generate a keypair and perform encrypt/decrypt
    auto kv = Encrypt::rsa_generate_keypair_pem(2048);
    std::string pub = kv.first;
    std::string priv = kv.second;

    // Quick sanity checks on PEM headers
    BOOST_CHECK_NE(pub.find("-----BEGIN PUBLIC KEY-----"), std::string::npos);
    BOOST_CHECK_NE(priv.find("-----BEGIN PRIVATE KEY-----"), std::string::npos);

    std::vector<unsigned char> plain = {7, 8, 9, 10, 11, 12};
    auto cipher = Encrypt::rsa_public_encrypt_pem(pub, plain);
    auto out = Encrypt::rsa_private_decrypt_pem(priv, cipher);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), out.begin(), out.end());
}

// The password-only AES-256-CBC helpers derive key+iv internally from the
// password, so a same-password encrypt/decrypt round-trip must recover the
// exact plaintext (this drives the plain cbc functions, not the *_with_iv ones).
BOOST_AUTO_TEST_CASE(aes256_cbc_plain_roundtrip)
{
    std::string pass = "cbc-plain-pass";
    std::vector<unsigned char> plain = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17};
    auto cipher = Encrypt::aes256_cbc_encrypt(plain, pass);
    // CBC adds PKCS#7 padding, so a non-block-multiple input grows to the next block boundary
    BOOST_CHECK_EQUAL(cipher.size(), 32u);
    auto out = Encrypt::aes256_cbc_decrypt(cipher, pass);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), out.begin(), out.end());

    // Exact block-multiple input still round-trips (padding appends a full block)
    std::vector<unsigned char> aligned(16);
    for (int i = 0; i < 16; ++i)
        aligned[i] = static_cast<unsigned char>(0xA0 + i);
    auto out2 = Encrypt::aes256_cbc_decrypt(Encrypt::aes256_cbc_encrypt(aligned, pass), pass);
    BOOST_REQUIRE_EQUAL_COLLECTIONS(aligned.begin(), aligned.end(), out2.begin(), out2.end());
}

// The *_with_iv helpers validate the IV length up front and reject a mismatched
// IV with std::invalid_argument before touching OpenSSL (AES iv = 16, 3DES iv = 8).
BOOST_AUTO_TEST_CASE(iv_length_validation)
{
    std::string pass = "iv-validate";
    std::vector<unsigned char> data = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};

    const std::vector<unsigned char> short_aes_iv(15, 0);
    const std::vector<unsigned char> long_aes_iv(17, 0);
    BOOST_CHECK_THROW(Encrypt::aes256_cbc_encrypt_with_iv(data, pass, short_aes_iv), std::invalid_argument);
    BOOST_CHECK_THROW(Encrypt::aes256_cbc_decrypt_with_iv(data, pass, long_aes_iv), std::invalid_argument);

    const std::vector<unsigned char> short_des3_iv(7, 0);
    const std::vector<unsigned char> long_des3_iv(9, 0);
    BOOST_CHECK_THROW(Encrypt::des3_cbc_encrypt_with_iv(data, pass, short_des3_iv), std::invalid_argument);
    BOOST_CHECK_THROW(Encrypt::des3_cbc_decrypt_with_iv(data, pass, long_des3_iv), std::invalid_argument);
}

// The RSA helpers read PEM through OpenSSL BIOs; a malformed PEM string makes the
// PEM_read_bio_* step fail and surface as RuntimeError (deterministic error branch).
BOOST_AUTO_TEST_CASE(rsa_invalid_pem_throws)
{
    std::vector<unsigned char> data = {1, 2, 3, 4};
    const std::string garbage = "this is not a PEM encoded key";
    BOOST_CHECK_THROW(Encrypt::rsa_public_encrypt_pem(garbage, data), RuntimeError);
    BOOST_CHECK_THROW(Encrypt::rsa_private_decrypt_pem(garbage, data), RuntimeError);
}

BOOST_AUTO_TEST_CASE(md5_hash)
{
    std::string input = "The quick brown fox jumps over the lazy dog";
    auto hash = Hasher::md5_hex(input);
    std::string expected_hex = "9e107d9d372bb6826bd81d3542a419d6";
    BOOST_REQUIRE_EQUAL(expected_hex, hash);
}

// The string_view overloads and *_to_string helpers of Encoder are thin inline
// wrappers around the vector/byte primitives already covered above; these cases
// drive them directly so the header's inline bodies are exercised.
BOOST_AUTO_TEST_CASE(encoder_string_view_wrappers)
{
    // base64_encode(string_view) must match base64_encode(vector of the same bytes)
    const std::string text = "Hello, HsBaSlicer!";
    std::vector<unsigned char> bytes(text.begin(), text.end());
    const auto b64_from_view = Encoder::base64_encode(std::string_view(text));
    BOOST_REQUIRE_EQUAL(b64_from_view, Encoder::base64_encode(bytes));

    // base64_decode_to_string round-trips back to the original text
    BOOST_REQUIRE_EQUAL(Encoder::base64_decode_to_string(b64_from_view), text);

    // hex_encode(string_view) and hex_decode_to_string round-trip
    const auto hex_from_view = Encoder::hex_encode(std::string_view(text));
    BOOST_REQUIRE_EQUAL(hex_from_view, Encoder::hex_encode(bytes));
    BOOST_REQUIRE_EQUAL(Encoder::hex_decode_to_string(hex_from_view), text);

    // empty input stays empty for the decoding helpers and the hex encoder
    // (base64_encode rejects a zero-length write by design, so it is not tested here)
    BOOST_REQUIRE(Encoder::base64_decode_to_string("").empty());
    BOOST_REQUIRE_EQUAL(Encoder::hex_encode(std::string_view("")), "");
    BOOST_REQUIRE(Encoder::hex_decode_to_string("").empty());
}

BOOST_AUTO_TEST_SUITE_END()
