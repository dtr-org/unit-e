# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/MIT.
#
# Run tests with `pytest`, requires `pytest-mock` to be installed.

from unittest.mock import call
import os

from pathlib import Path

gitian_build = __import__('gitian-build')

class Log:
    def __init__(self, name):
        self.name = name
        self.create()

    def filename(self):
        return "test_data/gitian_build/" + self.name + ".log"

    def create(self):
        with open(self.filename(), "w") as file:
            file.write("Test: %s\n" % self.name)

    def log(self, line):
        with open(self.filename(), "a") as file:
            file.write(line + "\n")

    def check(self):
        with open(self.filename()) as file:
            with open(self.filename() + ".expected") as file_expected:
                assert file.read() == file_expected.read()

    def log_call(self, parameters, shell=None, stdout=None, stderr=None, universal_newlines=None, encoding=None, cwd=None):
        if isinstance(parameters, list):
            line = " ".join([str(param) for param in parameters])
        else:
            line = parameters
        if shell:
            line += "  shell=" + str(shell)
        if cwd:
            line += "  cwd=" + str(Path(cwd).resolve().relative_to(Path('.').resolve()))
        if stdout:
            line += "  stdout=" + str(stdout)
        if stderr:
            line += "  stderr=" + str(stdout)
        if universal_newlines:
            line += "  universal_newlines=" + str(universal_newlines)
        if encoding:
            line += "  encoding=" + str(encoding)
        self.log(line)
        if parameters[0:2] == ["git", "show"]:
            return "somecommit"
        else:
            return 0

    def log_chdir(self, parameter):
        self.log("chdir('%s')" % parameter)

def create_args(mocker):
    args = mocker.Mock()
    args.version = "someversion"
    args.jobs = "2"
    args.memory = "3000"
    args.commit = "v1.1.1"
    args.url = "/some/repo"
    args.git_dir = "unit-e"
    args.sign_prog = "gpg"
    args.signer = "somesigner"
    return args

# Mock find_osslsigncode function to return the user_spec path
# Test the function separately
def find_osslsigncode_mock(user_spec):
    return user_spec

original_find_osslsigncode = gitian_build.find_osslsigncode
gitian_build.find_osslsigncode = find_osslsigncode_mock

def test_build(mocker):
    log = Log("test_build")

    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("os.makedirs")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("os.getcwd", return_value="somedir")

    gitian_build.build(create_args(mocker), "someworkdir")

    log.check()

def test_sign(mocker):
    log = Log("test_sign")

    class TemporaryDirectoryMock():
        def __init__(self, dirname):
            self.dirname = dirname
        def __enter__(self):
            return self.dirname
        def __exit__(self, *args):
            pass

    # glob.glob has to return a generator, so ['...', '...'] is not enough
    mocker.patch("glob.glob", return_value=(f for f in ['unite-someversion-win32-setup-unsigned.exe', 'unite-someversion-win32-setup-unsigned.exe']))
    mocker.patch("tempfile.TemporaryDirectory", return_value=TemporaryDirectoryMock("tmp"))
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    args = create_args(mocker)
    args.osslsigncode_path = 'osslsigncode_executable'
    args.win_code_cert_path = 'somecert'
    args.win_code_key_path = 'somekey'

    gitian_build.sign(args)

    log.check()

def test_verify(mocker):
    log = Log("test_verify")

    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    gitian_build.verify(create_args(mocker), "someworkdir")

    log.check()

def test_setup_linux(mocker):
    log = Log("test_setup_linux")

    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)

    gitian_build.setup(create_args(mocker))

    log.check()

def test_prepare_git_dir(mocker):
    log = Log("test_prepare_git_dir")

    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.check_output", side_effect=log.log_call)

    gitian_build.prepare_git_dir(create_args(mocker), "someworkdir")

    log.check()

def test_apt_wrapper(mocker):
    log = Log("test_apt_wrapper")

    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)

    apt = gitian_build.Apt(quiet=False)
    apt.add_requirements('package_1')
    apt.add_requirements('package_2a', 'package_2b')
    apt.try_to_install('package_3')
    apt.updated = False
    apt.batch_install()

    apt = gitian_build.Apt(quiet=True)
    apt.is_installed = lambda p: False
    apt.add_requirements('package_1')
    apt.add_requirements('package_2a', 'package_2b')
    apt.batch_install()
    apt.try_to_install('package_3')

    log.check()

def test_find_osslsigncode(mocker):

    class which_mock:
        def __init__(self, rc, rp=''):
            self.rc = rc
            self.rp = rp
        def wait(self):
            return self.rc
        def communicate(self):
            return (self.rp.encode()+b'\n', b'')


    mocker.patch("subprocess.Popen", return_value=which_mock(1))
    mocker.patch("subprocess.call", return_value=255)

    # won't find the osslsigncode
    mocker.patch("pathlib.Path.is_file", return_value=False)
    assert(original_find_osslsigncode("") is None)

    # will find the osslsigncode in osslsigncode-1.7.1/osslsigncode
    mocker.patch("pathlib.Path.is_file", return_value=True)
    assert(Path('osslsigncode-1.7.1/osslsigncode').resolve() == original_find_osslsigncode(""))


    mocker.patch("subprocess.call", return_value=1)

    # won't find the osslsigncode
    mocker.patch("pathlib.Path.is_file", return_value=False)
    assert(original_find_osslsigncode("") is None)

    # will find the osslsigncode in osslsigncode-1.7.1/osslsigncode but won't be able to execute it (--version won't return 255)
    mocker.patch("pathlib.Path.is_file", return_value=True)
    assert(original_find_osslsigncode("") is None)

    # Tests user specified path
    # user-specified path does not return 255 on --version
    mocker.patch("pathlib.Path.is_file", return_value=False)
    try:
        original_find_osslsigncode("somepath")
        assert(False)
    except SystemExit as e:
        pass

    # user-specified path does not return 255 on --version
    mocker.patch("pathlib.Path.is_file", return_value=True)
    try:
        original_find_osslsigncode("somepath")
        assert(False)
    except SystemExit as e:
        pass

    # Should return the path passed in
    mocker.patch("pathlib.Path.is_file", return_value=True)
    mocker.patch("subprocess.call", return_value=255)
    assert(os.path.basename(original_find_osslsigncode("somepath")) == "somepath")

    # check which () output
    mocker.patch("subprocess.Popen", return_value=which_mock(0, '/someosslpath'))
    assert(original_find_osslsigncode("") == '/someosslpath')

    mocker.patch("subprocess.call", return_value=1)
    mocker.patch("subprocess.Popen", return_value=which_mock(0, '/someosslpath'))
    assert(original_find_osslsigncode("") is None)

