// Copyright (c) 2019 The Unit-e developers
// Distributed under the MIT software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.

#include <test/test_unite_block_fixture.h>

#include <streams.h>

namespace {

//! \brief Creates a real block with nine transactions.
//!
//! The block here is derived from a real unite block at height=100002:
//! 0000000000013b8ab2cd513b0261a14096412195a72a0c4827d229dcc7e0f7af
CBlock GetRealBlock() {
  CBlock block;
  // UNIT-E: Added a block signature with zero bytes (0x00 appended)
  // this might just be an interim solution to make the test pass
  CDataStream stream(
      ParseHex(
          // block header

          // block version
          "01000000"
          // prev block
          "90f0a9f110702f808219ebea1173056042a714bad51b916cb680000000000000"
          // hash merkle root
          "5275289558f51c9966699404ae2294730c3c9f9bda53523ce50e9b95e558da2f"
          // hash witness merkle root
          "0000000000000000000000000000000000000000000000000000000000000000"
          // time
          "db261b4d"
          // bits
          "4c86041b"
          // nonce
          "1ab1bf93"

          // number of transactions
          "09"

          // tx[0] coinbase transaction

          // tx version
          "01000000"
          // number of inputs
          "01"
          // prevout.hash
          "0000000000000000000000000000000000000000000000000000000000000000"
          // prevout.n
          "ffffffff"
          // script sig
          "07044c86041b0146"
          // sequence
          "ffffffff"
          // number of outputs
          "01"
          // amount (block reward)
          "00f2052a01000000"
          // script pub key
          "434104e18f7afbe4721580e81e8414fc8c24d7cfacf254bb5c7b949450c3e997"
          "c2dc1242487a8169507b631eb3771f2b425483fb13102c4eb5d858eef260fe70"
          "fbfae0ac"
          // transaction lock time
          "00000000"

          // tx[1]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "96608ccbafa16abada902780da4dc35dafd7af05fa0da08cf833575f8cf9e836"
          "000000004a493046022100dab24889213caf43ae6adc41cf1c9396c08240c199"
          "f5225acf45416330fd7dbd022100fe37900e0644bf574493a07fc5edba06dbc0"
          "7c311b947520c2d514bc5725dcb401ffffffff0100f2052a010000001976a914"
          "f15d1921f52e4007b146dfa60f369ed2fc393ce288ac"
          // transaction locktime
          "00000000"

          // tx[2]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "fb766c1288458c2bafcfec81e48b24d98ec706de6b8af7c4e3c29419bfacb56d"
          "000000008c493046022100f268ba165ce0ad2e6d93f089cfcd3785de5c963bb5"
          "ea6b8c1b23f1ce3e517b9f022100da7c0f21adc6c401887f2bfd1922f11d7615"
          "9cbc597fbd756a23dcbb00f4d7290141042b4e8625a96127826915a5b1098526"
          "36ad0da753c9e1d5606a50480cd0c40f1f8b8d898235e571fe9357d9ec842bc4"
          "bba1827daaf4de06d71844d0057707966affffffff0280969800000000001976"
          "a9146963907531db72d0ed1a0cfb471ccb63923446f388ac80d6e34c00000000"
          "1976a914f0688ba1c0d1ce182c7af6741e02658c7d4dfcd388ac"
          // transaction locktime
          "00000000"

          // tx[3]

          // tx version
          "01000000"
          // number of inputs
          "02"
          // remaining transaction data
          "c40297f730dd7b5a99567eb8d27b78758f607507c52292d02d4031895b52f2ff"
          "010000008b483045022100f7edfd4b0aac404e5bab4fd3889e0c6c41aa8d0e6f"
          "a122316f68eddd0a65013902205b09cc8b2d56e1cd1f7f2fafd60a129ed94504"
          "c4ac7bdc67b56fe67512658b3e014104732012cb962afa90d31b25d8fb0e32c9"
          "4e513ab7a17805c14ca4c3423e18b4fb5d0e676841733cb83abaf975845c9f6f"
          "2a8097b7d04f4908b18368d6fc2d68ecffffffffca5065ff9617cbcba45eb237"
          "26df6498a9b9cafed4f54cbab9d227b0035ddefb000000008a47304402206801"
          "0362a13c7f9919fa832b2dee4e788f61f6f5d344a7c2a0da6ae7406056580220"
          "06d1af525b9a14a35c003b78b72bd59738cd676f845d1ff3fc25049e01003614"
          "014104732012cb962afa90d31b25d8fb0e32c94e513ab7a17805c14ca4c3423e"
          "18b4fb5d0e676841733cb83abaf975845c9f6f2a8097b7d04f4908b18368d6fc"
          "2d68ecffffffff01001ec4110200000043410469ab4181eceb28985b9b4e895c"
          "13fa5e68d85761b7eee311db5addef76fa8621865134a221bd01f28ec9999ee3"
          "e021e60766e9d1f3458c115fb28650605f11c9ac"
          // transaction locktime
          "00000000"

          // tx[4]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "cdaf2f758e91c514655e2dc50633d1e4c84989f8aa90a0dbc883f0d23ed5c2fa"
          "010000008b48304502207ab51be6f12a1962ba0aaaf24a20e0b69b27a94fac5a"
          "df45aa7d2d18ffd9236102210086ae728b370e5329eead9accd880d0cb070aea"
          "0c96255fae6c4f1ddcce1fd56e014104462e76fd4067b3a0aa42070082dcb0bf"
          "2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812"
          "b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff02404b4c00000000001976a9"
          "142b6ba7c9d796b75eef7942fc9288edd37c32f5c388ac002d31010000000019"
          "76a9141befba0cdc1ad56529371864d9f6cb042faa06b588ac"
          // transaction locktime
          "00000000"

          // tx[5]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "b4a47603e71b61bc3326efd90111bf02d2f549b067f4c4a8fa183b57a0f800cb"
          "010000008a4730440220177c37f9a505c3f1a1f0ce2da777c339bd8339ffa02c"
          "7cb41f0a5804f473c9230220585b25a2ee80eb59292e52b987dad92acb0c64ec"
          "ed92ed9ee105ad153cdb12d001410443bd44f683467e549dae7d20d1d79cbdb6"
          "df985c6e9c029c8d0c6cb46cc1a4d3cf7923c5021b27f7a0b562ada113bc85d5"
          "fda5a1b41e87fe6e8802817cf69996ffffffff0280651406000000001976a914"
          "5505614859643ab7b547cd7f1f5e7e2a12322d3788ac00aa0271000000001976"
          "a914ea4720a7a52fc166c55ff2298e07baf70ae67e1b88ac"
          // transaction locktime
          "00000000"

          // tx[6]

          // tx version
          "01000000"
          // number of inputs
          "05"
          // remaining transaction data
          "86c62cd602d219bb60edb14a3e204de0705176f9022fe49a538054fb14abb49e"
          "010000008c493046022100f2bc2aba2534becbdf062eb993853a42bbbc282083"
          "d0daf9b4b585bd401aa8c9022100b1d7fd7ee0b95600db8535bbf331b19eed8d"
          "961f7a8e54159c53675d5f69df8c014104462e76fd4067b3a0aa42070082dcb0"
          "bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c8"
          "12b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffff03ad0e58ccdac3df9dc28a"
          "218bcf6f1997b0a93306faaa4b3a28ae83447b2179010000008b483045022100"
          "be12b2937179da88599e27bb31c3525097a07cdb52422d165b3ca2f2020ffcf7"
          "02200971b51f853a53d644ebae9ec8f3512e442b1bcb6c315a5b491d119d1062"
          "4c83014104462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f"
          "07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8"
          "ebbb12dcd4ffffffff2acfcab629bbc8685792603762c921580030ba144af553"
          "d271716a95089e107b010000008b483045022100fa579a840ac258871365dd48"
          "cd7552f96c8eea69bd00d84f05b283a0dab311e102207e3c0ee9234814cfbb1b"
          "659b83671618f45abc1326b9edcc77d552a4f2a805c0014104462e76fd4067b3"
          "a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c40fbd4b82963c69b"
          "3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dcd4ffffffffdcdc60"
          "23bbc9944a658ddc588e61eacb737ddf0a3cd24f113b5a8634c517fcd2000000"
          "008b4830450221008d6df731df5d32267954bd7d2dda2302b74c6c2a6aa5c0ca"
          "64ecbabc1af03c75022010e55c571d65da7701ae2da1956c442df81bbf076cdb"
          "ac25133f99d98a9ed34c014104462e76fd4067b3a0aa42070082dcb0bf2f388b"
          "6495cf33d789904f07d0f55c40fbd4b82963c69b3dc31895d0c772c812b1d5fb"
          "cade15312ef1c0e8ebbb12dcd4ffffffffe15557cd5ce258f479dfd6dc6514ed"
          "f6d7ed5b21fcfa4a038fd69f06b83ac76e010000008b483045022023b3e0ab07"
          "1eb11de2eb1cc3a67261b866f86bf6867d4558165f7c8c8aca2d86022100dc6e"
          "1f53a91de3efe8f63512850811f26284b62f850c70ca73ed5de8771fb4510141"
          "04462e76fd4067b3a0aa42070082dcb0bf2f388b6495cf33d789904f07d0f55c"
          "40fbd4b82963c69b3dc31895d0c772c812b1d5fbcade15312ef1c0e8ebbb12dc"
          "d4ffffffff01404b4c00000000001976a9142b6ba7c9d796b75eef7942fc9288"
          "edd37c32f5c388ac"
          // transaction locktime
          "00000000"

          // tx[7]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "66d7577163c932b4f9690ca6a80b6e4eb001f0a2fa9023df5595602aae96ed8d"
          "000000008a4730440220262b42546302dfb654a229cefc86432b89628ff259dc"
          "87edd1154535b16a67e102207b4634c020a97c3e7bbd0d4d19da6aa2269ad9dd"
          "ed4026e896b213d73ca4b63f014104979b82d02226b3a4597523845754d44f13"
          "639e3bf2df5e82c6aab2bdc79687368b01b1ab8b19875ae3c90d661a3d0a3316"
          "1dab29934edeb36aa01976be3baf8affffffff02404b4c00000000001976a914"
          "4854e695a02af0aeacb823ccbc272134561e0a1688ac40420f00000000001976"
          "a914abee93376d6b37b5c2940655a6fcaf1c8e74237988ac"
          // transaction locktime
          "00000000"

          // tx[8]

          // tx version
          "01000000"
          // number of inputs
          "01"
          // remaining transaction data
          "4e3f8ef2e91349a9059cb4f01e54ab2597c1387161d3da89919f7ea6acdbb371"
          "010000008c49304602210081f3183471a5ca22307c0800226f3ef9c353069e07"
          "73ac76bb580654d56aa523022100d4c56465bdc069060846f4fbf2f6b20520b2"
          "a80b08b168b31e66ddb9c694e240014104976c79848e18251612f8940875b2b0"
          "8d06e6dc73b9840e8860c066b7e87432c477e9a59a453e71e6d76d5fe34058b8"
          "00a098fc1740ce3012e8fc8a00c96af966ffffffff02c0e1e400000000001976"
          "a9144134e75a6fcb6042034aab5e18570cf1f844f54788ac404b4c0000000000"
          "1976a9142b6ba7c9d796b75eef7942fc9288edd37c32f5c388ac"
          // transaction locktime
          "00000000"

          // block signature of zero bytes
          "00"),
      SER_NETWORK, PROTOCOL_VERSION);
  stream >> block;
  return block;
}

}  // namespace

RealBlockFixture::RealBlockFixture() : block(GetRealBlock()) {}
