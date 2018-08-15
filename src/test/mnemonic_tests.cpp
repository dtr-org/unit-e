// Copyright (c) 2017 The Particl developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <key/mnemonic/mnemonic.h>

#include <test/data/bip39_vectors_english.json.h>
#include <test/data/bip39_vectors_japanese.json.h>

#include <test/test_unite.h>

#include <key.h>
#include <base58.h>
#include <util.h>

#include <string>
#include <boost/test/unit_test.hpp>

#include <univalue.h>

extern UniValue read_json(const std::string& jsondata);

BOOST_FIXTURE_TEST_SUITE(mnemonic_tests, BasicTestingSetup)

BOOST_AUTO_TEST_CASE(mnemonic_test)
{
    std::string words = "deer clever bitter bonus unable menu satoshi chaos dwarf inmate robot drama exist nuclear raise";
    std::string expect_seed = "1da563986981b82c17a76160934f4b532eac77e14b632c6adcf31ba4166913e063ce158164c512cdce0672cbc9256dd81e7be23a8d8eb331de1a497493c382b1";

    std::vector<uint8_t> vSeed;
    std::string password = "";
    BOOST_CHECK(0 == key::mnemonic::ToSeed(words, password, vSeed));

    BOOST_CHECK(HexStr(vSeed.begin(), vSeed.end()) == expect_seed);
}

BOOST_AUTO_TEST_CASE(mnemonic_test_fails)
{
    // Fail tests

    std::string sError;
    std::vector<uint8_t> vEntropy;
    std::string sWords = "legals winner thank year wave sausage worth useful legal winner thank yellow";
    BOOST_CHECK_MESSAGE(3 == key::mnemonic::Decode(key::mnemonic::Language::ENGLISH, sWords, vEntropy, sError), "MnemonicDecode: " << sError);

    sWords = "winner legal thank year wave sausage worth useful legal winner thank yellow";
    BOOST_CHECK_MESSAGE(5 == key::mnemonic::Decode(key::mnemonic::Language::ENGLISH, sWords, vEntropy, sError), "MnemonicDecode: " << sError);
}

BOOST_AUTO_TEST_CASE(mnemonic_addchecksum)
{
    std::string sError;
    std::string sWordsIn = "abandon baby cabbage dad eager fabric gadget habit ice kangaroo lab";
    std::string sWordsOut;

    BOOST_CHECK_MESSAGE(0 == key::mnemonic::AddChecksum(key::mnemonic::Language::ENGLISH, sWordsIn, sWordsOut, sError), "MnemonicAddChecksum: " << sError);

    BOOST_CHECK_MESSAGE(sWordsOut == "abandon baby cabbage dad eager fabric gadget habit ice kangaroo lab absorb", "sWordsOut: " << sWordsOut);

    // Must fail, len % 3 != 0
    std::string sWordsInFail = "abandon baby cabbage dad eager fabric gadget habit ice kangaroo";
    BOOST_CHECK_MESSAGE(4 == key::mnemonic::AddChecksum(key::mnemonic::Language::ENGLISH, sWordsInFail, sWordsOut, sError), "MnemonicAddChecksum: " << sError);


    std::string sWordsInFrench = "zoologie ficeler xénon voyelle village viande vignette sécréter séduire torpille remède";

    BOOST_CHECK(0 == key::mnemonic::AddChecksum(key::mnemonic::Language::FRENCH, sWordsInFrench, sWordsOut, sError));

    BOOST_CHECK(sWordsOut == "zoologie ficeler xénon voyelle village viande vignette sécréter séduire torpille remède abolir");
}

BOOST_AUTO_TEST_CASE(mnemonic_detect_english)
{
    std::string mnemonic = "abandon baby cabbage dad eager fabric gadget habit ice kangaroo";
    boost::optional<key::mnemonic::Language> maybeLanguage = key::mnemonic::DetectLanguage(mnemonic);
    BOOST_CHECK(maybeLanguage != boost::none);
    key::mnemonic::Language language = maybeLanguage.get();
    BOOST_CHECK_EQUAL((int) language, (int) key::mnemonic::Language::ENGLISH);
}

BOOST_AUTO_TEST_CASE(mnemonic_detect_french)
{
    std::string mnemonic = "tortue lessive rocheux trancher breuvage souvenir agencer enjeu pluie dicter système jubiler pantalon fixer fébrile";
    boost::optional<key::mnemonic::Language> maybeLanguage = key::mnemonic::DetectLanguage(mnemonic);
    BOOST_CHECK(maybeLanguage != boost::none);
    key::mnemonic::Language language = maybeLanguage.get();
    BOOST_CHECK_EQUAL((int) language, (int) key::mnemonic::Language::FRENCH);
}

BOOST_AUTO_TEST_CASE(mnemonic_detect_italian)
{
    std::string mnemonic = "truccato obelisco sipario uccello cadetto tabacco allievo fondente rompere endemico tigella negozio remoto indagine idrico";
    boost::optional<key::mnemonic::Language> maybeLanguage = key::mnemonic::DetectLanguage(mnemonic);
    BOOST_CHECK(maybeLanguage != boost::none);
    key::mnemonic::Language language = maybeLanguage.get();
    BOOST_CHECK_EQUAL((int) language, (int) key::mnemonic::Language::ITALIAN);
}

BOOST_AUTO_TEST_CASE(mnemonic_detect_spanish)
{
    std::string mnemonic = "trauma menú salón triste bronce taquilla alacrán fallo prole domingo texto manta pesa guardia glaciar";
    boost::optional<key::mnemonic::Language> maybeLanguage = key::mnemonic::DetectLanguage(mnemonic);
    BOOST_CHECK(maybeLanguage != boost::none);
    key::mnemonic::Language language = maybeLanguage.get();
    BOOST_CHECK_EQUAL((int) language, (int) key::mnemonic::Language::SPANISH);
}

BOOST_AUTO_TEST_CASE(mnemonic_detect_korean)
{
    std::string mnemonic = u8"학과 여동생 창구 학습 깜빡 탤런트 거액 봉투 점원 바닷가 판매 양배추 작은딸 선택 색깔";
    boost::optional<key::mnemonic::Language> maybeLanguage = key::mnemonic::DetectLanguage(mnemonic);
    BOOST_CHECK(maybeLanguage != boost::none);
    key::mnemonic::Language language = maybeLanguage.get();
    BOOST_CHECK_EQUAL((int) language, (int) key::mnemonic::Language::KOREAN);
}

BOOST_AUTO_TEST_CASE(mnemonic_seed_english)
{
    key::mnemonic::Seed seed("leopard cycle economy main denial rebuild local panther dentist raise cry story trade agree despair");
    BOOST_CHECK_EQUAL(seed.GetLanguageTag(), "english");
    BOOST_CHECK_EQUAL(seed.GetHexSeed(), "030eda9ac4bc2ed71cc55b41c2b9d735c93dae05e0316b07b2bd66abdc851af0f0c0309d4be8c63788f88f4ae6d509f4d60302bf5319bf1968b173995514628f");
    BOOST_CHECK_EQUAL(seed.GetExtKey58().ToString(), "xprv9s21ZrQH143K396rQ3kSpYY3gBxLWU45UHwtqWvy5MmbZrdpkfB3bRwKtfxN3KY39pKMM5icEupwjFiNdxPrXA1ggVCymVYGnQMh6pRDAAg");
}

BOOST_AUTO_TEST_CASE(mnemonic_seed_english_with_passphrase)
{
    key::mnemonic::Seed seed("unit mind spell upper cart thumb always feel rotate echo town mask random habit goddess", "batteryhorsestaple");

    BOOST_CHECK_EQUAL(seed.GetLanguageTag(), "english");
    BOOST_CHECK_EQUAL(seed.GetHexSeed(), "0d063ec29046dc315a1ce49773b2b126e0038a0f0f0d3eb9f752c28d7aa041034e1ec6f30e8af2afb6f1f8673f0303aca0b1333be4041211284c4e7a659ee96d");
    BOOST_CHECK_EQUAL(seed.GetExtKey58().ToString(), "xprv9s21ZrQH143K3gCHrPaaDLEH3nfveAMMNqJg7AWGBm7zEefXn4eaU6LpquEVqitXBKRJexzVoVYwPQtf4bPX8xP8YhFrWr5cQg58zAk3iuu");
}

BOOST_AUTO_TEST_CASE(mnemonic_seed_spanish)
{
    key::mnemonic::Seed seed("trauma menú salón triste bronce taquilla alacrán fallo prole domingo texto manta pesa guardia glaciar");

    BOOST_CHECK_EQUAL(seed.GetLanguageTag(), "spanish");
    BOOST_CHECK_EQUAL(seed.GetHexSeed(), "f88d237dfba9c4b440bf75eece3430a6ded113565c839fe29b9f0c0efa46cfe972d8cb35be7d43f0f8000fb7f8d7de085a2f4ab8c71d96249d48e2532fe7a245");
    BOOST_CHECK_EQUAL(seed.GetExtKey58().ToString(), "xprv9s21ZrQH143K2FkTcmESR4PsC96smZegsSZfCexcBBTwFEA7nUeuGeNyEddAXWSxHRW7aNpBPPofmbH8a9jQwapak4557qBUKt6f5pRvR3H");
}

BOOST_AUTO_TEST_CASE(mnemonic_seed_spanish_with_passphrase)
{
    key::mnemonic::Seed seed("trauma menú salón triste bronce taquilla alacrán fallo prole domingo texto manta pesa guardia glaciar", "batteryhorsestaple");

    BOOST_CHECK_EQUAL(seed.GetLanguageTag(), "spanish");
    BOOST_CHECK_EQUAL(seed.GetHexSeed(), "c5b03b324e35b950928e7d62bcae6354c2a5292036edfca3600611f680fa1d0608f95b800731bd827a1c2c7f681b188f8cbeebcd9122689d009f3bd1818df355");
    BOOST_CHECK_EQUAL(seed.GetExtKey58().ToString(), "xprv9s21ZrQH143K3N4wNaBjjvrxrnrqPQV3h1cfrXC5mD71SUr3dFSYxQBzWZea6GU9SgFMm6WTKVg9W7TkAYuQ4iUUo3n7ygKW3njJaGie34q");
}

void runTests(key::mnemonic::Language language, UniValue &tests)
{
    std::string sError;
    for (unsigned int idx = 0; idx < tests.size(); idx++)
    {
        UniValue test = tests[idx];

        assert(test.size() > 2);

        std::string sEntropy = test[0].get_str();
        std::string sWords = test[1].get_str();
        std::string sSeed;
        std::string sPassphrase;
        if (test.size() > 3)
        {
            sPassphrase = test[2].get_str();
            sSeed = test[3].get_str();
        }
        else
        {
            sPassphrase = "TREZOR";
            sSeed = test[2].get_str();
        }

        std::vector<uint8_t> vEntropy = ParseHex(sEntropy);
        std::vector<uint8_t> vEntropyTest;

        std::string sWordsTest;
        BOOST_CHECK_MESSAGE(0 == key::mnemonic::Encode(language, vEntropy, sWordsTest, sError), "MnemonicEncode: " << sError);

        BOOST_CHECK(sWords == sWordsTest);

        BOOST_CHECK_MESSAGE(0 == key::mnemonic::Decode(language, sWords, vEntropyTest, sError), "MnemonicDecode: " << sError);
        BOOST_CHECK(vEntropy == vEntropyTest);

        std::vector<uint8_t> vSeed = ParseHex(sSeed);
        std::vector<uint8_t> vSeedTest;

        BOOST_CHECK(0 == key::mnemonic::ToSeed(sWords, sPassphrase, vSeedTest));
        BOOST_CHECK(vSeed == vSeedTest);
    }
}

BOOST_AUTO_TEST_CASE(mnemonic_test_json)
{
    UniValue tests_english = read_json(
        std::string(json_tests::bip39_vectors_english,
        json_tests::bip39_vectors_english + sizeof(json_tests::bip39_vectors_english)));

    runTests(key::mnemonic::Language::ENGLISH, tests_english);

    UniValue tests_japanese = read_json(
        std::string(json_tests::bip39_vectors_japanese,
        json_tests::bip39_vectors_japanese + sizeof(json_tests::bip39_vectors_japanese)));

    runTests(key::mnemonic::Language::JAPANESE, tests_japanese);
}

BOOST_AUTO_TEST_SUITE_END()
