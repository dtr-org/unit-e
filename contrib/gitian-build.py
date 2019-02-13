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

import tempfile
import glob

from pathlib import Path
from contextlib import contextmanager

OSSLSIGNCODE_VER = '1.7.1'
OSSLSIGNCODE_DIR = 'osslsigncode-'+OSSLSIGNCODE_VER

@contextmanager
def cd(dir):
    original_dir = os.getcwd()
    os.chdir(dir)
    try:
        yield
    finally:
        os.chdir(original_dir)

class Installer:
    """ Wrapper around native package installer, supports apt-get and brew, only
        installs packages which aren't installed yet."""
    def __init__(self, backend=None, quiet=False):
        if backend != "apt" and backend != "brew":
            raise ValueError("Invalid value for backend argument: '%'. Valid values are `apt` and `brew`.")
        self.backend = backend
        self.updated = False
        self.to_install = []
        if quiet and self.backend == "apt":
            self.flags = ['-qq']
        else:
            self.flags = []

    def backend_command(self, subcommand):
        if self.backend == 'apt':
            if subcommand == 'ls':
                command = ['dpkg', '-s']
            else:
                command = ['sudo', 'apt-get', subcommand] + self.flags
        elif self.backend == 'brew':
            command = ['brew', subcommand]
        return command

    def update(self):
        if not self.updated:
            self.updated = True
            subprocess.check_call(self.backend_command('update'))

    def try_to_install(self, *programs):
        self.update()
        print(self.backend + ': installing', ", ".join(programs))
        return subprocess.call(self.backend_command('install') + list(programs)) == 0

    def batch_install(self):
        if not self.to_install:
            print(self.backend + ': nothing to install')
            return

        if not self.try_to_install(*self.to_install):
            print('Could not install packages.', file=sys.stderr)
            exit(1)
        self.to_install = []

    def is_installed(self, program):
        return subprocess.call(self.backend_command('ls') + [program], stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL) == 0

    def add_requirements(self, *programs):
        for program in programs:
            if not self.is_installed(program):
                self.to_install.append(program)


def verify_user_specified_osslsigncode(user_spec_path):
    if not Path(user_spec_path).is_file():
        raise Exception('provided osslsign does not exists: {}'.format(user_spec_path))

    try:
        if subprocess.call([user_spec_path, '--version'], stderr=subprocess.DEVNULL) != 255:
            raise Exception('cannot execute provided osslsigncode: {}'.format(user_spec_path))
    except subprocess.CalledProcessError as e:
        raise Exception('unexpected exception raised while executing provided osslsigncode: {}'.format(str(e)))

    return Path(user_spec_path).resolve()

def find_osslsigncode(user_spec_path):
    if user_spec_path:
        return verify_user_specified_osslsigncode(user_spec_path)

    which_ossl_proc = subprocess.Popen(['which', 'osslsigncode'], stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    if which_ossl_proc.wait() == 0:
        ossl_path = which_ossl_proc.communicate()[0].split()[0].decode()
        if subprocess.call([ossl_path, '--version'], stderr=subprocess.DEVNULL) == 255:
            return ossl_path

    expected_path = Path(OSSLSIGNCODE_DIR, 'osslsigncode')
    if expected_path.is_file(expected_path) and subprocess.call([expected_path, '--version'], stderr=subprocess.DEVNULL) == 255:
        return expected_path.resolve()

    return None

def install_osslsigner():
    subprocess.check_call(['wget', '-N', 'https://downloads.sourceforge.net/project/osslsigncode/osslsigncode/osslsigncode-'+OSSLSIGNCODE_VER+'.tar.gz'])
    subprocess.check_call(['wget', '-N', 'https://bitcoincore.org/cfields/osslsigncode-Backports-to-'+OSSLSIGNCODE_VER+'.patch'])
    subprocess.check_call(["echo 'a8c4e9cafba922f89de0df1f2152e7be286aba73f78505169bc351a7938dd911 osslsigncode-Backports-to-"+OSSLSIGNCODE_VER+".patch' | sha256sum -c"], shell=True)
    subprocess.check_call(["echo 'f9a8cdb38b9c309326764ebc937cba1523a3a751a7ab05df3ecc99d18ae466c9 osslsigncode-"+OSSLSIGNCODE_VER+".tar.gz' | sha256sum -c"], shell=True)
    subprocess.check_call(['tar', '-xzf', 'osslsigncode-'+OSSLSIGNCODE_VER+'.tar.gz'])
    subprocess.check_call(['patch -p1 < ../osslsigncode-Backports-to-'+OSSLSIGNCODE_VER+'.patch'], shell=True, cwd=OSSLSIGNCODE_DIR)
    subprocess.check_call(['./configure', '--without-gsf', '--without-curl', '--disable-dependency-tracking'], cwd=OSSLSIGNCODE_DIR)
    subprocess.check_call(['make'], cwd=OSSLSIGNCODE_DIR)
    return Path(OSSLSIGNCODE_DIR, 'osslsigncode').resolve()


def install_libssl_dev(apt):
    dist_str = platform.dist()
    dist_type = dist_str[0]
    dist_no = dist_str[1].replace('.', '')
    if dist_type == 'Ubuntu' and dist_no < 1800 or dist_type == 'Debian' and dist_no < 900:
        apt.add_requirements('libssl-dev')
    else:
        apt.add_requirements('libssl1.0-dev')


def install_linux_deps(args):
    apt = Installer(backend='apt', quiet=args.quiet)
    apt.add_requirements('ruby', 'git', 'make')
    if args.kvm:
        apt.add_requirements('apt-cacher-ng', 'python-vm-builder', 'qemu-kvm', 'qemu-utils')
    elif args.docker:
        if subprocess.call(['docker', '--version']) != 0:
            if not apt.try_to_install('docker.io') and not apt.try_to_install('docker-ce'):
                print('Cannot find any way to install docker', file=sys.stderr)
                exit(1)
    else:
        apt.add_requirements('apt-cacher-ng', 'lxc', 'debootstrap')

    should_make_ossl = False
    if args.codesign and args.windows:
        args.osslsigncode_path = find_osslsigncode(args.osslsigncode_path)
        if not args.osslsigncode_path:
            should_make_ossl = True
            apt.add_requirements('tar', 'wget', 'patch', 'autoconf')
            install_libssl_dev(apt)

    apt.batch_install()

    if should_make_ossl:
        args.osslsigncode_path = install_osslsigner()

def create_bin_symlink(link, target):
    bin_path = Path("bin", link)
    if not bin_path.exists():
        bin_path.symlink_to(target)

def install_mac_deps(args):
    if not args.docker:
        print('Mac can only work with docker, re-run with --docker flag.', file=sys.stderr)
        exit(1)

    if subprocess.call(['docker', '--version']) != 0:
        print('Please install docker manually, e.g. with `brew cask install docker`.', file=sys.stderr)
        exit(1)

    brew = Installer(backend='brew')
    brew.add_requirements('ruby', 'coreutils')
    brew.batch_install()

    create_bin_symlink("date", "/usr/local/bin/gdate")
    create_bin_symlink("sha256sum", "/usr/local/bin/gsha256sum")

def install_deps(args):
    system_str = platform.system()
    if system_str == 'Linux':
        install_linux_deps(args)
    elif system_str == 'Darwin':
        install_mac_deps(args)
    else:
        print("Unsupported system '%s'." % system_str, file=sys.stderr)
        exit(1)


def setup(args):
    install_deps(args)

    if not Path('unit-e-sigs').is_dir():
        subprocess.check_call(['git', 'clone', 'git@github.com:dtr-org/unit-e-sigs.git'])
    if not Path('gitian-builder').is_dir():
        subprocess.check_call(['git', 'clone', 'https://github.com/devrandom/gitian-builder.git'])
    if not Path(args.git_dir).is_dir():
        subprocess.check_call(['git', 'clone', args.url, args.git_dir])
    make_image_prog = ['bin/make-base-vm', '--suite', 'trusty', '--arch', 'amd64']
    if args.docker:
        make_image_prog += ['--docker']
    elif not args.kvm:
        make_image_prog += ['--lxc']
    subprocess.check_call(make_image_prog, cwd='gitian-builder')
    if args.is_bionic and not args.kvm and not args.docker:
        subprocess.check_call(['sudo', 'sed', '-i', 's/lxcbr0/br0/', '/etc/default/lxc-net'])
        print('Reboot is required')
        exit(0)

def gitian_descriptors(args, platform_str):
    return '../work/gitian-descriptors/gitian-' + platform_str + '.yml'

def build(args):
    os.makedirs('unit-e-binaries/' + args.version, exist_ok=True)
    print('\nBuilding Dependencies\n')
    with cd('gitian-builder'):
        os.makedirs('inputs', exist_ok=True)

        subprocess.check_call(['make', '-C', Path('../', args.git_dir, 'depends'), 'download', 'SOURCES_PATH=' + os.getcwd() + '/cache/common'])

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

    if args.commit_files:
        print('\nCommitting '+args.version+' Unsigned Sigs\n')
        with cd('unit-e-sigs'):
            subprocess.check_call(['git', 'add', args.version+'-linux/'+args.signer])
            subprocess.check_call(['git', 'add', args.version+'-win-unsigned/'+args.signer])
            subprocess.check_call(['git', 'add', args.version+'-osx-unsigned/'+args.signer])
            subprocess.check_call(['git', 'commit', '-m', 'Add '+args.version+' unsigned sigs for '+args.signer])

def get_signatures_path(platform_str, version):
    return Path('unit-e-sigs', version + '-detached', 'unite-'+platform_str+'-signatures.tar.gz').resolve()


def codesign_windows(osslsign_path, version, win_code_cert_path, win_code_key_path):
    gitian_dir = Path('gitian-builder').resolve()
    signatures_tarball = get_signatures_path('win', version)
    if signatures_tarball.is_file():
        print('Signatures already present at:', signatures_tarball, '\nI cowardly refuse to continue', file=sys.stderr)
        exit(1)

    signatures_tarball.parent.mkdir(exist_ok=True)

    print('\nSigning ' + version + ' Windows')
    with tempfile.TemporaryDirectory() as build_dir:
        subprocess.check_call(['cp', 'inputs/unite-' + version + '-win-unsigned.tar.gz', Path(build_dir, 'unite-win-unsigned.tar.gz')], cwd=gitian_dir)
        subprocess.check_call(['tar', '-xzf', 'unite-win-unsigned.tar.gz'], cwd=build_dir)

        for fp in Path(build_dir).glob('unsigned/*.exe'):
            subprocess.check_call([osslsign_path, 'sign', '-certs', win_code_cert_path, '-in', fp, '-out', str(fp)+'-signed', '-key', win_code_key_path, '-askpass'])
            subprocess.check_call([osslsign_path, 'extract-signature', '-pem', '-in', str(fp)+'-signed', '-out', str(fp)+'.pem'])

        subprocess.check_call(['tar', '-czf', signatures_tarball, *[path.name for path in Path(build_dir, 'unsigned').glob('*.exe.pem')]], cwd=Path(build_dir, 'unsigned'))


def codesign_osx(version, signer, git_dir):
    gitian_dir = Path('gitian-builder').resolve()
    if platform.system() != 'Darwin':
        print('Codesigning MacOS binaries is only possible on a MacOS', file=sys.stderr)
        exit(1)

    signatures_tarball = get_signatures_path('osx', version)
    if signatures_tarball.is_file():
        print('Signatures already present at:', signatures_tarball, '\nI cowardly refuse to continue', file=sys.stderr)
        exit(1)

    with tempfile.TemporaryDirectory() as build_dir:
        subprocess.check_call(['cp', 'inputs/unite-' + version + '-osx-unsigned.tar.gz', Path(build_dir, 'unite-osx-unsigned.tar.gz')], cwd=gitian_dir)
        subprocess.check_call(['cp', Path(git_dir, 'contrib/macdeploy/detached-sig-create.sh'), build_dir])
        subprocess.check_call(['tar', '-xzf', 'unite-osx-unsigned.tar.gz'], cwd=build_dir)
        subprocess.check_call(['detached-sig-create.sh', '-s', signer], cwd=build_dir)
        subprocess.check_call(['cp', 'signature-osx.tar.gz', signatures_tarball], cwd=build_dir)


def codesign(args):
    if args.windows:
        codesign_windows(args.osslsigncode_path, args.version, args.win_code_cert_path, args.win_code_key_path)

    if args.macos:
        codesign_osx(args.version, args.signer, args.git_dir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Detached Sigs\n')
        subprocess.check_call(['git', 'add', Path(args.version + '-detached', 'unite-win-signatures.tar.gz')], cwd='unit-e-sigs')
        subprocess.check_call(['git', 'add', Path(args.version + '-detached', 'unite-osx-signatures.tar.gz')], cwd='unit-e-sigs')
        subprocess.check_call(['git', 'commit', '-a', '-m', 'Add '+args.version+' detached signatures by '+args.signer], cwd='unit-e-sigs')


def sign(args):
    gitian_dir = Path('gitian-builder').resolve()

    if args.windows:
        subprocess.check_call(['wget', '-N', '-P', 'inputs', 'https://downloads.sourceforge.net/project/osslsigncode/osslsigncode/osslsigncode-'+OSSLSIGNCODE_VER+'.tar.gz'], cwd=gitian_dir)
        subprocess.check_call(['wget', '-N', '-P', 'inputs', 'https://bitcoincore.org/cfields/osslsigncode-Backports-to-'+OSSLSIGNCODE_VER+'.patch'], cwd=gitian_dir)

        signatures_tarball = get_signatures_path('win', args.version)
        if not signatures_tarball.is_file():
            print('Signatures not present at:', signatures_tarball, file=sys.stderr)
            exit(1)

        print('\nSigning ' + args.version + ' Windows')
        subprocess.check_call(['cp', signatures_tarball, 'inputs/unite-win-signatures.tar.gz'], cwd=gitian_dir)
        subprocess.check_call(['cp', 'inputs/unite-' + args.version + '-win-unsigned.tar.gz', 'inputs/unite-win-unsigned.tar.gz'], cwd=gitian_dir)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature=master', gitian_descriptors(args, 'win-signer')], cwd=gitian_dir)
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-win-signed', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'win-signer')], cwd=gitian_dir)
        subprocess.check_call('mv build/out/unite-*win64-setup.exe ../unit-e-binaries/'+args.version, shell=True, cwd=gitian_dir)
        subprocess.check_call('mv build/out/unite-*win32-setup.exe ../unit-e-binaries/'+args.version, shell=True, cwd=gitian_dir)

    if args.macos:
        signatures_tarball = get_signatures_path('osx', args.version)
        if not signatures_tarball.is_file():
            print('Signatures not present at:', signatures_tarball, file=sys.stderr)
            exit(1)

        print('\nSigning ' + args.version + ' MacOS')
        subprocess.check_call(['cp', signatures_tarball, 'inputs/unite-osx-signatures.tar.gz'], cwd=gitian_dir)
        subprocess.check_call(['cp', Path('..', args.git_dir, 'contrib/macdeploy/detached-sig-apply.sh'), 'inputs/detached-sig-apply.sh'], cwd=gitian_dir)
        subprocess.check_call(['cp inputs/unite-' + args.version + '-osx-unsigned.tar.gz', 'inputs/unite-osx-unsigned.tar.gz'], cwd=gitian_dir)
        subprocess.check_call(['bin/gbuild', '-i', '--commit', 'signature=master', gitian_descriptors(args, 'osx-signer')], cwd=gitian_dir)
        subprocess.check_call(['bin/gsign', '-p', args.sign_prog, '--signer', args.signer, '--release', args.version+'-osx-signed', '--destination', '../unit-e-sigs/', gitian_descriptors(args, 'osx-signer')], cwd=gitian_dir)
        subprocess.check_call('mv build/out/unite-osx-signed.dmg ../unit-e-binaries/'+args.version+'/unite-'+args.version+'-osx.dmg', shell=True, cwd=gitian_dir)

    if args.commit_files:
        print('\nCommitting '+args.version+' Signed Sigs\n')
        subprocess.check_call(['git', 'add', args.version+'-win-signed/'+args.signer], cwd='unit-e-sigs')
        subprocess.check_call(['git', 'add', args.version+'-osx-signed/'+args.signer], cwd='unit-e-sigs')
        subprocess.check_call(['git', 'commit', '-a', '-m', 'Add '+args.version+' signed binary sigs for '+args.signer], cwd='unit-e-sigs')


def verify(args):
    """ Verify the builds. Exits with error in case any of the signatures fail to verify and if any of the signatures are missing. """
    gitian_builder = Path('gitian-builder')
    sigs_path = Path('unit-e-sigs')

    builds_were_missing = False

    for (sig_path_suffix, descriptor_suffix, build_name) in [
        ('linux', 'linux', 'Linux'),
        ('win-unsigned', 'win', 'Windows'),
        ('osx-unsigned', 'osx', 'MacOS'),
        ('win-signed', 'win-signer', 'Signed Windows'),
        ('osx-signed', 'osx-signer', 'Signed Max OS')
    ]:
        build_sig_dir = args.version + '-' + sig_path_suffix
        if Path(sigs_path, build_sig_dir).is_dir():
            print('\nVerifying v{} {}\n'.format(args.version, build_name))
            descriptor = gitian_descriptors(args, descriptor_suffix)
            subprocess.check_call(['bin/gverify', '-v', '-d', '../unit-e-sigs/', '-r', build_sig_dir, descriptor], cwd=gitian_builder)
        else:
            print('\nSkipping v{} {} as it is not present\n'.format(args.version, build_name))
            builds_were_missing = True

    if builds_were_missing:
        print('Some builds were missing, please refer to previous logs.', file=sys.stderr)
        exit(1)


def prepare_git_dir(args):
    with cd(args.git_dir):
        if args.pull:
            subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
            os.chdir('../gitian-builder/inputs/unit-e')
            subprocess.check_call(['git', 'fetch', args.url, 'refs/pull/'+args.version+'/merge'])
            args.commit = subprocess.check_output(['git', 'show', '-s', '--format=%H', 'FETCH_HEAD'], universal_newlines=True, encoding='utf8').strip()
            args.version = 'pull-' + args.version
        print(args.commit)
        if not args.skip_checkout:
            subprocess.check_call(['git', 'fetch'])
            subprocess.check_call(['git', 'checkout', args.commit])

# Use only keyword-only arguments as defined in PEP 3102 to avoid accidentally swapping of arguments
def prepare_gitian_descriptors(*, source, target, hosts=None):
    descriptor_source_dir = Path(source)
    descriptor_dir = Path(target)
    if not descriptor_source_dir.is_dir():
        raise Exception("Gitian descriptor dir '%s' does not exist" % descriptor_source_dir)
    descriptor_dir.mkdir(parents=True, exist_ok=True)
    for descriptor_path in descriptor_source_dir.glob("*.yml"):
        filename = descriptor_path.relative_to(descriptor_source_dir)
        descriptor_in = descriptor_source_dir / filename
        descriptor_out = descriptor_dir / filename
        with descriptor_out.open("w") as file_out, descriptor_in.open() as file_in:
            for line in file_in:
                if hosts and line.startswith('  HOSTS='):
                    file_out.write('  HOSTS="%s"\n' % hosts)
                else:
                    file_out.write(line)

def create_work_dir(args):
    work_dir = Path(args.work_dir)
    if Path(args.work_dir).exists() and not Path(args.work_dir).is_dir():
        raise Exception("Work dir '%s' exists but is not a directory." % work_dir)
    if not work_dir.exists():
        print("Creating working directory '%s'" % work_dir)
        work_dir.mkdir()
    return work_dir.resolve()

def main():
    parser = argparse.ArgumentParser(usage='%(prog)s [options]')
    parser.add_argument('-c', '--commit', action='store_true', dest='commit', help='Indicate that the version argument is for a commit or branch')
    parser.add_argument('-p', '--pull', action='store_true', dest='pull', help='Indicate that the version argument is the number of a github repository pull request')
    parser.add_argument('-u', '--url', dest='url', default='git@github.com:dtr-org/unit-e.git', help='Specify the URL of the repository. Default is %(default)s')
    parser.add_argument('-g', '--git-dir', dest='git_dir', default='unit-e', help='Specify the name of the directory where the unit-e git repo is checked out. This is relative to the working directory (see --work-dir option)')
    parser.add_argument('-v', '--verify', action='store_true', dest='verify', help='Verify the Gitian build')
    parser.add_argument('-b', '--build', action='store_true', dest='build', help='Do a Gitian build')
    parser.add_argument('-C', '--codesign', action='store_true', dest='codesign', help='Make detached signatures for Windows and MacOS')
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
    parser.add_argument('--osslsign', dest='osslsigncode_path', help='Path to osslsigncode executable.')
    parser.add_argument('--win-code-cert', dest='win_code_cert_path', help='Path to code signing certificate.')
    parser.add_argument('--win-code-key', dest='win_code_key_path', help='Path to key corresponding to code signing certificate.')
    parser.add_argument('--signer', dest='signer', help='GPG signer to sign each build assert file. Required to build, sign or verify')
    parser.add_argument('--version', dest='version', help='Version number, commit, or branch to build. If building a commit or branch, the -c option must be specified. Required to build, sign or verify')
    parser.add_argument('--skip-checkout', action='store_true', help='Skip checkout of git repo. Use with care as it might leave you in an inconsistent state.')
    parser.add_argument('--hosts', dest='hosts', help='Specify hosts for which is built. See the gitian descriptors for valid values.')
    parser.add_argument('-w', '--work-dir', dest='work_dir', help='Working directory where data is checked out and the build takes place. The directory will be created if it does not exist.')
    args = parser.parse_args()

    if not args.work_dir:
        print("Please provide a working directory (--work-dir <path>)", file=sys.stderr)
        exit(1)

    work_dir = create_work_dir(args)
    with cd(work_dir):
        print("Using working directory '%s'" % work_dir)

        os.environ["PATH"] = str(Path("bin").resolve()) + ":" + os.environ["PATH"]

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
        if args.macos and not Path('gitian-builder/inputs/MacOSX10.11.sdk.tar.gz').is_file():
            print('Cannot build for MacOS, SDK does not exist. Will build for other OSes')
            args.macos = False

        if args.setup:
            setup(args)

        if not args.build and not args.sign and not args.verify and not args.codesign:
            return

        script_name = Path(sys.argv[0]).name

        # Signer and version shouldn't be empty
        if not args.signer:
            print(script_name+': Missing signer.')
            print('Try '+script_name+' --help for more information')
            exit(1)

        if not args.version:
            print(script_name+': Missing version.')
            print('Try '+script_name+' --help for more information')
            exit(1)

        # Check that path to osslsigncode executable is known
        if args.windows and args.codesign:
            if not args.setup:
                args.osslsigncode_path = find_osslsigncode(args.osslsigncode_path)

            if not args.osslsigncode_path:
                print('Cannot find osslsigncode. Please provide it with --osslsign or run --setup to make it.', file=sys.stderr)
                exit(1)

            if not args.win_code_cert_path or not Path(args.win_code_cert_path).is_file():
                print('Please provide code signing certificate path (--code-cert)', file=sys.stderr)
                exit(1)

            if not args.win_code_key_path or not Path(args.win_code_key_path).is_file():
                print('Please provide code signing key path (--code-key)', file=sys.stderr)
                exit(1)


        # Add leading 'v' for tags
        if args.commit and args.pull:
            raise Exception('Cannot have both commit and pull')
        args.commit = ('' if args.commit else 'v') + args.version

        if args.pull and args.skip_checkout:
            raise Exception('Cannot pull and skip-checkout at the same time')

        prepare_git_dir(args)
        prepare_gitian_descriptors(source=args.git_dir + "/contrib/gitian-descriptors/", target="work/gitian-descriptors",
                                hosts=args.hosts)

        if args.build:
            build(args)

        if args.codesign:
            codesign(args)

        if args.sign:
            sign(args)

        if args.verify:
            verify(args)

if __name__ == '__main__':
    main()
