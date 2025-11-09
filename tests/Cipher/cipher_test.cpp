#define BOOST_TEST_MODULE cipher_test
#include <boost/test/included/unit_test.hpp>

#include <vector>
#include <string>

#include "cipher/encoder.hpp"
#include "cipher/encrypt.hpp"
#include "cipher/hasher.hpp"

#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <openssl/bn.h>

using namespace HsBa::Slicer::Cipher;

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
		EVP_PKEY_assign_RSA(pkey, rsa); // pkey owns rsa now

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

		return { pub_pem, priv_pem };
	}
}

BOOST_AUTO_TEST_SUITE(cipher_tests)

BOOST_AUTO_TEST_CASE(encoder_roundtrip)
{
	std::vector<unsigned char> data = { 1,2,3,4,5,6,7,8,9 };
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
	std::vector<unsigned char> plain = { 10,20,30,40,50,60,70,80 };

	auto c = Encrypt::aes256_ecb_encrypt(plain, pass);
	auto p = Encrypt::aes256_ecb_decrypt(c, pass);
	BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p.begin(), p.end());

	std::vector<unsigned char> iv(16);
	for (int i = 0; i < 16; ++i) iv[i] = static_cast<unsigned char>(i + 1);
	auto c2 = Encrypt::aes256_cbc_encrypt_with_iv(plain, pass, iv);
	auto p2 = Encrypt::aes256_cbc_decrypt_with_iv(c2, pass, iv);
	BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p2.begin(), p2.end());
}

BOOST_AUTO_TEST_CASE(des3_roundtrip)
{
	std::string pass = "3despass";
	std::vector<unsigned char> plain = { 5,4,3,2,1,9,8,7 };

	auto c = Encrypt::des3_ecb_encrypt(plain, pass);
	auto p = Encrypt::des3_ecb_decrypt(c, pass);
	BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p.begin(), p.end());

	std::vector<unsigned char> iv(8);
	for (int i = 0; i < 8; ++i) iv[i] = static_cast<unsigned char>(i + 1);
	auto c2 = Encrypt::des3_cbc_encrypt_with_iv(plain, pass, iv);
	auto p2 = Encrypt::des3_cbc_decrypt_with_iv(c2, pass, iv);
	BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), p2.begin(), p2.end());
}

BOOST_AUTO_TEST_CASE(rsa_roundtrip)
{
	auto pair = generate_rsa_pem_pair();
	std::string pub = pair.first;
	std::string priv = pair.second;

	std::vector<unsigned char> plain = { 11,22,33,44,55 };
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

	std::vector<unsigned char> plain = { 7,8,9,10,11,12 };
	auto cipher = Encrypt::rsa_public_encrypt_pem(pub, plain);
	auto out = Encrypt::rsa_private_decrypt_pem(priv, cipher);
	BOOST_REQUIRE_EQUAL_COLLECTIONS(plain.begin(), plain.end(), out.begin(), out.end());
}

BOOST_AUTO_TEST_CASE(md5_hash)
{
	std::string input = "The quick brown fox jumps over the lazy dog";
	auto hash = Hasher::md5_hex(input);
	std::string expected_hex = "9e107d9d372bb6826bd81d3542a419d6";
	BOOST_REQUIRE_EQUAL(expected_hex, hash);
}

BOOST_AUTO_TEST_SUITE_END()
