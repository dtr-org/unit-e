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

def install_linux_deps():
    global args
    subprocess.check_call(['sudo', 'apt-get', 'update', '-qq'])
    programs = ['ruby', 'git', 'make', 'wget']
    if args.kvm:
        programs += ['apt-cacher-ng', 'python-vm-builder', 'qemu-kvm', 'qemu-utils']
    elif args.docker:
        if subprocess.call(['docker', '--version']) != 0:
            dockers = ['docker.io', 'docker-ce']
            for i in dockers:
                return_code = subprocess.call(['sudo', 'apt-get', 'install', '-qq', i])
                if return_code == 0:
                    break
            if return_code != 0:
                print('Cannot find any way to install docker', file=sys.stderr)
                exit(1)
    else:
        programs += ['apt-cacher-ng', 'lxc', 'debootstrap']
    programs_to_be_installed = []
    for program in programs:
        if subprocess.call(['dpkg', '-s', program], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 1:
            programs_to_be_installed.append(program)
    if programs_to_be_installed:
        print('Installing ' + ", ".join(programs_to_be_installed))
        subprocess.check_call(['sudo', 'apt-get', 'update', '-qq'])
        subprocess.check_call(['sudo', 'apt-get', 'install', '-qq'] + programs_to_be_installed)

def install_mac_deps():
    global args
    if not args.docker:
        print('Mac can only work with docker, re-run with --docker flag.', file=sys.stderr)
        exit(1)

    if subprocess.call(['docker', '--version']) != 0:
        # TODO: check if it's enough (docker-machine?)
        if subprocess.call(['brew', 'install', 'docker']) != 0:
            print('Please install docker manually.', file=sys.stderr)
            exit(1)

    subprocess.check_call(['brew', 'install', 'ruby', 'coreutils'])

def install_deps():
    global args
    system_str = platform.system()
    if system_str == 'Linux':
        install_linux_deps()
    elif system_str == 'Darwin':
        install_mac_deps()
    else:
        print('Unsupported system.', file=sys.stderr)
        exit(1)


def setup():
    global args, workdir

    install_deps()

    if not os.path.isdir('unit-e-sigs'):
        subprocess.check_call(['git', 'clone', 'git@github.com:dtr-org/unit-e-sigs.git'])
    if not os.path.isdir('gitian-builder'):
        subprocess.check_call(['git', 'clone', 'https://github.com/devrandom/gitian-builder.git'])
    if not os.path.isdir('unit-e'):
        subprocess.check_call(['git', 'clone', args.url, 'unit-e'])
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

def build():
    global args, workdir

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
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, '../unit-e/contrib/gitian-descriptors/gitian-linux.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-linux', '--destination', '../unit-e-sigs/', '../unit-e/contrib/gitian-descriptors/gitian-linux.yml'])
        subprocess.check_call('mv build/out/unite-*.tar.gz build/out/src/unite-*.tar.gz ../unit-e-binaries/'+args.version, shell=True)

    if args.windows:
        print('\nCompiling ' + args.version + ' Windows')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, '../unit-e/contrib/gitian-descriptors/gitian-win.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-unsigned', '--destination', '../unit-e-sigs/', '../unit-e/contrib/gitian-descriptors/gitian-win.yml'])
        subprocess.check_call('mv build/out/unite-*-win-unsigned.tar.gz inputs/', shell=True)
        subprocess.check_call('mv build/out/unite-*.zip build/out/unite-*.exe ../unit-e-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nCompiling ' + args.version + ' MacOS')
        subprocess.check_call(['bin/gbuild', '-j', args.jobs, '-m', args.memory, '--commit', 'unit-e='+args.commit, '--url', 'unit-e='+args.url, '../unit-e/contrib/gitian-descriptors/gitian-osx.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-unsigned', '--destination', '../unit-e-sigs/', '../unit-e/contrib/gitian-descriptors/gitian-osx.yml'])
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

def sign():
    global args, workdir
    os.chdir('gitian-builder')

    if args.windows:
        print('\nSigning ' + args.version + ' Windows')
        subprocess.check_call('cp inputs/unite-' + args.version + '-win-unsigned.tar.gz inputs/unite-win-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, '../unit-e/contrib/gitian-descriptors/gitian-win-signer.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-signed', '--destination', '../unit-e-sigs/detached', '../unit-e/contrib/gitian-descriptors/gitian-win-signer.yml'])
        subprocess.check_call('mv build/out/unite-*win64-setup.exe ../unit-e-binaries/'+args.version, shell=True)
        subprocess.check_call('mv build/out/unite-*win32-setup.exe ../unit-e-binaries/'+args.version, shell=True)

    if args.macos:
        print('\nSigning ' + args.version + ' MacOS')
        subprocess.check_call('cp inputs/unite-' + args.version + '-osx-unsigned.tar.gz inputs/unite-osx-unsigned.tar.gz', shell=True)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature='+args.commit, '../unit-e/contrib/gitian-descriptors/gitian-osx-signer.yml'])
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-signed', '--destination', '../unit-e-sigs/detached', '../unit-e/contrib/gitian-descriptors/gitian-osx-signer.yml'])
        subprocess.check_call('mv build/out/unite-osx-signed.dmg ../unit-e-binaries/'+args.version+'/unite-'+args.version+'-osx.dmg', shell=True)

    os.chdir(workdir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Signed Sigs\n')
        os.chdir('unit-e-sigs')
        subprocess.check_call(['git', 'add', args.version+'-win-signed/'+args.signer])
        subprocess.check_call(['git', 'add', args.version+'-osx-signed/'+args.signer])
        subprocess.check_call(['git', 'commit', '-a', '-m', 'Add '+args.version+' signed binary sigs for '+args.signer])
        os.chdir(workdir)

def verify():
    global args, workdir
    os.chdir('gitian-builder')

    print('\nVerifying v'+args.version+' Linux\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-linux', '../unit-e/contrib/gitian-descriptors/gitian-linux.yml'])
    print('\nVerifying v'+args.version+' Windows\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-win-unsigned', '../unit-e/contrib/gitian-descriptors/gitian-win.yml'])
    print('\nVerifying v'+args.version+' MacOS\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-osx-unsigned', '../unit-e/contrib/gitian-descriptors/gitian-osx.yml'])
    print('\nVerifying v'+args.version+' Signed Windows\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-win-signed', '../unit-e/contrib/gitian-descriptors/gitian-win-signer.yml'])
    print('\nVerifying v'+args.version+' Signed MacOS\n')
    subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', args.version+'-osx-signed', '../unit-e/contrib/gitian-descriptors/gitian-osx-signer.yml'])

    os.chdir(workdir)

def main():
    global args, workdir

    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument('-c', '--commit', action='store_true', dest='commit', help='Indicate that the version argument is for a commit or branch')
    parser.add_argument('-p', '--pull', action='store_true', dest='pull', help='Indicate that the version argument is the number of a github repository pull request')
    parser.add_argument('-u', '--url', dest='url', default='git@github.com:dtr-org/unit-e.git', help='Specify the URL of the repository. Default is %(default)s')
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
        setup()

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

    os.chdir('unit-e')
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

    if args.build:
        build()

    if args.sign:
        sign()

    if args.verify:
        verify()

if __name__ == '__main__':
    main()
