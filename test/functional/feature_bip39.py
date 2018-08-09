#!/usr/bin/env python3
# Copyright (c) 2014-2017 The Bitcoin Core developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or http://www.opensource.org/licenses/mit-license.php.
"""Test running united with the -rpcbind and -rpcallowip options."""

from test_framework.test_framework import UnitETestFramework

class BIP39Test(UnitETestFramework):

    def __init__(self):
        super().__init__()

        self.__seeds = {
            "english": "unit mind spell upper cart thumb always feel rotate echo town mask random habit goddess",
            "japanese": "もくし　たまる　ひんそう　やさしい　おじぎ　まなぶ　いきなり　こもん　のいず　けなみ　みやげ　たおす　にっけい　しゃいん　しつもん",
            "spanish": "trauma menú salón triste bronce taquilla alacrán fallo prole domingo texto manta pesa guardia glaciar",
            "chinese_s": "赴 倾 酵 雏 压 肌 民 让 矩 止 册 阴 喝 矛 愿",
            "chinese_t": "赴 傾 酵 雛 壓 肌 民 讓 矩 止 冊 陰 喝 矛 願",
            "french": "tortue lessive rocheux trancher breuvage souvenir agencer enjeu pluie dicter système jubiler pantalon fixer fébrile",
            "italian": "truccato obelisco sipario uccello cadetto tabacco allievo fondente rompere endemico tigella negozio remoto indagine idrico",
            "korean": "학과 여동생 창구 학습 깜빡 탤런트 거액 봉투 점원 바닷가 판매 양배추 작은딸 선택 색깔",
        }

        self.__passphrases = [
            None,
            "",
            "something"
        ]

    def set_test_params(self):
        self.num_nodes = 1

    def setup_network(self):
        self.add_nodes(self.num_nodes, None)

    def run_test(self):
        for lang, seed in self.__seeds:
            for passphrase in self.__passphrases:
                self._test_mnemonicinfo_language(seed, passphrase, lang)

    def _test_mnemonicinfo_language(self, seed, passphrase, language):
        assert(self.nodes[0].mnemonicinfo(seed, passphrase).language_tag == language)
