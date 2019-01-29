#!/usr/bin/env python3
# Copyright (c) 2018-2019 The Bitcoin Core developers
# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/MIT.

import argparse
import os
import subprocess
import sys

import platform


class Apt:
    """ Lazy apt wrapper """
    def __init__(self, quiet=False):
        self.updated = False
        self.to_install = []
        if quiet:
            self.apt_flags = ['-qq']
        else:
            self.apt_flags = []

    def update(self):
        if not self.updated:
            self.updated = True
            subprocess.check_call(['sudo', 'apt-get', 'update'] + self.apt_flags)

    def try_to_install(self, *programs):
        self.update()
        print('Apt: installing', ", ".join(programs))
        return subprocess.call(['sudo', 'apt-get', 'install'] + self.apt_flags + list(programs)) == 0

    def batch_install(self):
        if not self.to_install:
            print('Apt: nothing to install')
            return

        if not self.try_to_install(*self.to_install):
            print('Could not install packages.')
            exit(1)
        self.to_install = []

    def is_installed(self, program):
        return subprocess.call(['dpkg', '-s', program], stdout=subprocess.DEVNULL) == 0

    def add_requirements(self, *programs):
        for program in programs:
            if not self.is_installed(program):
                self.to_install.append(program)

def install_linux_deps(args):
    apt = Apt(args.quiet)
    apt.add_requirements('ruby', 'git', 'make', 'wget')
    if args.kvm:
        apt.add_requirements('apt-cacher-ng', 'python-vm-builder', 'qemu-kvm', 'qemu-utils')
    elif args.docker:
        if subprocess.call(['docker', '--version']) != 0:
            if not apt.try_to_install('docker.io') and not apt.try_to_install('docker-ce'):
                print('Cannot find any way to install docker', file=sys.stderr)
                exit(1)
    else:
        apt.add_requirements('apt-cacher-ng', 'lxc', 'debootstrap')

    apt.batch_install()

def install_mac_deps(args):
    if not args.docker:
        print('Mac can only work with docker, re-run with --docker flag.', file=sys.stderr)
        exit(1)

    if subprocess.call(['docker', '--version']) != 0:
        # TODO: check if it's enough (docker-machine?)
        if subprocess.call(['brew', 'install', 'docker']) != 0:
            print('Please install docker manually.', file=sys.stderr)
            exit(1)

    subprocess.check_call(['brew', 'install', 'ruby', 'coreutils'])

def install_deps(args):
    system_str = platform.system()
    if system_str == 'Linux':
        install_linux_deps(args)
    elif system_str == 'Darwin':
        install_mac_deps(args)
    else:
        print('Unsupported system.', file=sys.stderr)
        exit(1)


def setup(args, workdir):
    install_deps(args)

    if not os.path.isdir('unit-e-sigs'):
        subprocess.check_call(['git', 'clone', 'git@github.com:dtr-org/unit-e-sigs.git'])
    if not os.path.isdir('gitian-builder'):
        subprocess.check_call(['git', 'clone', 'https://github.com/devrandom/gitian-builder.git'])
    if not os.path.isdir(args.git_dir):
        subprocess.check_call(['git', 'clone', args.url, args.git_dir])
    os.chdir('gitian-builder')
    make_image_prog = ['bin/make-base-vm', '--suite', 'trusty', '--arch', 'amd64']
    if args.docker:
        make_image_prog += ['--docker']
    elif not args.kvm:
        make_image_prog += ['--lxc']
    subprocess.check_call(make_image_prog)
    os.chdir(workdir)
    if args.is_bionic and not args.kvm and not args.docker:
        subprocess.check_call(['sudo', 'sed', '-i', 's/lxcbr0/br0/', '/etc/default/lxc-net'])
        print('Reboot is required')
        exit(0)

def prepare_gitian_descriptors(args):
    os.makedirs('build')

def gitian_descriptors(args, platform):
    return '../' + args.git_dir + '/contrib/gitian-descriptors/gitian-' + platform + '.yml'

def build(args, workdir):
    os.makedirs('unit-e-binaries/' + args.version, exist_ok=True)
    print('\nBuilding Dependencies\n')
    os.chdir('gitian-builder')
    os.makedirs('inputs', exist_ok=True)

    subprocess.check_call(['wget', '-N', '-P', 'inputs', 'https://downloads.sourceforge.net/project/osslsigncode/osslsigncode/osslsigncode-1.7.1.tar.gz'])
    subprocess.check_call(['wget', '-N', '-P', 'inputs', 'https://bitcoincore.org/cfields/osslsigncode-Backports-to-1.7.1.patch'])
    subprocess.check_call(["echo 'a8c4e9cafba922f89de0df1f2152e7be286aba73f78505169bc351a7938dd911 inputs/osslsigncode-Backports-to-1.7.1.patch' | sha256sum -c"], shell=True)
    subprocess.check_call(["echo 'f9a8cdb38b9c309326764ebc937cba1523a3a751a7ab05df3ecc99d18ae466c9 inputs/osslsigncode-1.7.1.tar.gz' | sha256sum -c"], shell=True)
    subprocess.check_call(['make', '-C', '../unit-e/depends', 'download', 'SOURCES_PATH=' + os.getcwd() + '/cache/common'])

    if args.linux:
        print('\nCompiling ' + args.version + ' Linux')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, gitian_descriptors(args, 'linux')])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-linux', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'linux')])
        subprocess.check_call('mv build/out/unite-*.tar.gz build/out/src/unite-*.tar.gz ../unit-e-binaries/'+args.version, shell=True)

    if args.windows:
        print('\nCompiling ' + args.version + ' Windows')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, gitian_descriptors(args, 'win')])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-unsigned', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'win')])
        subprocess.check_call('mv build/out/unite-*-win-unsigned.tar.gz inputs/', shell=True)
        subprocess.check_call('mv build/out/unite-*.zip build/out/unite-*.exe ../unit-e-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nCompiling ' + args.version + ' MacOS')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, gitian_descriptors(args, 'osx')])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-unsigned', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'osx')])
        subprocess.check_call('mv build/out/unite-*-osx-unsigned.tar.gz inputs/', shell=True)
        subprocess.check_call('mv build/out/unite-*.tar.gz build/out/unite-*.dmg ../unit-e-binaries/'+args.version, shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Unsigned Sigs\n')
        os.chdir('unit-e-sigs')
        subprocess.check_call(['git', 'add', args.version+'-linux/'+args.signer])
        subprocess.check_call(['git', 'add', args.version+'-win-unsigned/'+args.signer])
        subprocess.check_call(['git', 'add', args.version+'-osx-unsigned/'+args.signer])
        subprocess.check_call(['git', 'commit', '-m', 'Add '+args.version+' unsigned sigs for '+args.signer])
        os.chdir(workdir)

def sign(args, workdir):
    os.chdir('gitian-builder')

    if args.windows:
        print('\nSigning ' + args.version + ' Windows')
        subprocess.check_call('cp inputs/unite-' + args.version + '-win-unsigned.tar.gz inputs/unite-win-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, gitian_descriptors(args, 'win-signer')])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-signed', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'win-signer')])
        subprocess.check_call('mv build/out/unite-*win64-setup.exe ../unit-e-binaries/'+args.version, shell=True)
        subprocess.check_call('mv build/out/unite-*win32-setup.exe ../unit-e-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nSigning ' + args.version + ' MacOS')
        subprocess.check_call('cp inputs/unite-' + args.version + '-osx-unsigned.tar.gz inputs/unite-osx-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, gitian_descriptors(args, 'osx-signer')])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-signed', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'osx-signer')])
        subprocess.check_call('mv build/out/unite-osx-signed.dmg ../unit-e-binaries/'+args.version+'/unite-'+args.version+'-osx.dmg', shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Signed Sigs\n')
        os.chdir('unit-e-sigs')
        subprocess.check_call(['git', 'add', args.version+'-win-signed/'+args.signer])
        subprocess.check_call(['git', 'add', args.version+'-osx-signed/'+args.signer])
        subprocess.check_call(['git', 'commit', '-a', '-m', 'Add '+args.version+' signed binary sigs for '+args.signer])
        os.chdir(workdir)

def verify(args, workdir):
    os.chdir('gitian-builder')

    print('\nVerifying v'+args.version+' Linux\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-linux', gitian_descriptors(args, 'linux')])
    print('\nVerifying v'+args.version+' Windows\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-win-unsigned', gitian_descriptors(args, 'win')])
    print('\nVerifying v'+args.version+' MacOS\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-osx-unsigned', gitian_descriptors(args, 'osx')])
    print('\nVerifying v'+args.version+' Signed Windows\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-win-signed', gitian_descriptors(args, 'win-signer')])
    print('\nVerifying v'+args.version+' Signed MacOS\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-osx-signed', gitian_descriptors(args, 'osx-signer')])

    os.chdir(workdir)

def prepare_git_dir(args, workdir):
    os.chdir(args.git_dir)
    if args.pull:
        subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        os.chdir('../gitian-builder/inputs/unit-e')
        subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
        args.commit = subprocess.check_output(['git', 'show', '-s', '--format=%H', 'FETCH_HEAD'], universal_newlines=True, encoding='utf8').strip()
        args.version = 'pull-' + args.version
    print(args.commit)
    subprocess.check_call(['git', 'fetch'])
    subprocess.check_call(['git', 'checkout', args.commit])
    os.chdir(workdir)

def main():
    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument('-c', '--commit', action='store_true', dest='commit', help='Indicate that the version argument is for a commit or branch')
    parser.add_argument('-p', '--pull', action='store_true', dest='pull', help='Indicate that the version argument is the number of a github repository pull request')
    parser.add_argument('-u', '--url', dest='url', default='git@github.com:dtr-org/unit-e.git', help='Specify the URL of the repository. Default is %(default)s')
    parser.add_argument('-g', '--git-dir', dest='git_dir', default='unit-e', help='Specify the name of the directory where the unit-e git repo is checked out')
    parser.add_argument('-v', '--verify', action='store_true', dest='verify', help='Verify the Gitian build')
    parser.add_argument('-b', '--build', action='store_true', dest='build', help='Do a Gitian build')
    parser.add_argument('-s', '--sign', action='store_true', dest='sign', help='Make signed binaries for Windows and MacOS')
    parser.add_argument('-B', '--buildsign', action='store_true', dest='buildsign', help='Build both signed and unsigned binaries')
    parser.add_argument('-o', '--os', dest='os', default='lwm', help='Specify which Operating Systems the build is for. Default is %(default)s. l for Linux, w for Windows, m for MacOS')
    parser.add_argument('-j', '--jobs', dest='jobs', default='2', help='Number of processes to use. Default %(default)s')
    parser.add_argument('-m', '--memory', dest='memory', default='2000', help='Memory to allocate in MiB. Default %(default)s')
    parser.add_argument('-k', '--kvm', action='store_true', dest='kvm', help='Use KVM instead of LXC')
    parser.add_argument('-d', '--docker', action='store_true', dest='docker', help='Use Docker instead of LXC')
    parser.add_argument('-S', '--setup', action='store_true', dest='setup', help='Set up the Gitian building environment. Uses LXC. If you want to use KVM, use the --kvm option. Only works on Debian-based systems (Ubuntu, Debian)')
    parser.add_argument('-D', '--detach-sign', action='store_true', dest='detach_sign', help='Create the assert file for detached signing. Will not commit anything.')
    parser.add_argument('-n', '--no-commit', action='store_false', dest='commit_files', help='Do not commit anything to git')
    parser.add_argument('-q', '--quiet', action='store_true', dest='quiet', help='Do not prompt user for confirmations')
    parser.add_argument('--signer', dest='signer', help='GPG signer to sign each build assert file. Required to build, sign or verify')
    parser.add_argument('--version', dest='version', help='Version number, commit, or branch to build. If building a commit or branch, the -c option must be specified. Required to build, sign or verify')

    args = parser.parse_args()
    workdir = os.getcwd()

    args.linux = 'l' in args.os
    args.windows = 'w' in args.os
    args.macos = 'm' in args.os

    args.is_bionic = platform.dist() == ('Ubuntu', '18.04', 'bionic')

    if args.buildsign:
        args.build=True
        args.sign=True

    if args.kvm and args.docker:
        raise Exception('Error: cannot have both kvm and docker')

    args.sign_prog = 'true' if args.detach_sign else 'gpg --detach-sign'

    # Set environment variable USE_LXC or USE_DOCKER, let gitian-builder know that we use lxc or docker
    if args.docker:
        os.environ['USE_DOCKER'] = '1'
        os.environ['USE_LXC'] = ''
        os.environ['USE_VBOX'] = ''
    elif not args.kvm:
        os.environ['USE_LXC'] = '1'
        os.environ['USE_DOCKER'] = ''
        os.environ['USE_VBOX'] = ''
        if not 'GITIAN_HOST_IP' in os.environ.keys():
            os.environ['GITIAN_HOST_IP'] = '10.0.3.1'
        if not 'LXC_GUEST_IP' in os.environ.keys():
            os.environ['LXC_GUEST_IP'] = '10.0.3.5'

    # Disable for MacOS if no SDK found
    if args.macos and not os.path.isfile('gitian-builder/inputs/MacOSX10.11.sdk.tar.gz'):
        print('Cannot build for MacOS, SDK does not exist. Will build for other OSes')
        args.macos = False

    if args.setup:
        setup(args, workdir)

    if not args.build and not args.sign and not args.verify:
        return

    script_name = os.path.basename(sys.argv[0])

    # Signer and version shouldn't be empty
    if not args.signer:
        print(script_name+': Missing signer.')
        print('Try '+script_name+' --help for more information')
        exit(1)

    if not args.version:
        print(script_name+': Missing version.')
        print('Try '+script_name+' --help for more information')
        exit(1)

    # Add leading 'v' for tags
    if args.commit and args.pull:
        raise Exception('Cannot have both commit and pull')
    args.commit = ('' if args.commit else 'v') + args.version

    prepare_git_dir(args)

    if args.build:
        build(args, workdir)

    if args.sign:
        sign(args, workdir)

    if args.verify:
        verify(args, workdir)

if __name__ == '__main__':
    main()
