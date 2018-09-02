from random import randint
from test_framework.util import *
from test_framework.test_framework import UnitETestFramework


class CreateGenesisAddresses(UnitETestFramework):

    def set_test_params(self):
        self.num_nodes = 5

        self.setup_clean_chain = True

    def run_test(self):
        nodes = self.nodes

        nodes[0].importmasterkey('swap fog boost power mountain pair gallery crush price fiscal thing supreme chimney drastic grab acquire any cube cereal another jump what drastic ready')
        nodes[1].importmasterkey('chef gas expect never jump rebel huge rabbit venue nature dwarf pact below surprise foam magnet science sister shrimp blanket example okay office ugly')
        nodes[2].importmasterkey('narrow horror cheap tape language turn smart arch grow tired crazy squirrel sun pumpkin much panic scissors math pass tribe limb myself bone hat')
        nodes[3].importmasterkey('soon empty next roof proof scorpion treat bar try noble denial army shoulder foam doctor right shiver reunion hub horror push theme language fade')
        nodes[4].importmasterkey('seed innocent life live card rib volume until quiz swamp bid globe upper avoid jar ski lamp zone denial morning rhythm typical spider bronze')

        for node in nodes:
            addr = node.getnewaddress("", "legacy")
            script = node.validateaddress(addr)['scriptPubKey']
            print(script)

        return

def printSome(node):
    print(node.name)
    addr = node.getaccountaddress("")
    val = node.validateaddress(addr)
    print(val['address'])
    print(val['scriptPubKey'])
    print(val['pubkey'])
    print(val['addresses'][0])

if __name__ == '__main__':
    CreateGenesisAddresses().main()
