# Copyright (c) 2019 The Unit-e developers
# Distributed under the MIT software license, see the accompanying
# file COPYING or https://opensource.org/licenses/MIT.
#
# Run tests with `pytest`, requires `pytest-mock` to be installed.


from pytest import raises

from pathlib import Path

from subprocess import CalledProcessError

gitian_build = __import__('gitian-build')

class Log:
    def __init__(self, name):
        self.name = name
        self.create()

    def filename(self):
        return "test_data/gitian_build/" + self.name + ".log"

    def create(self):
        with open(self.filename(), "w", encoding="utf8") as file:
            file.write("Test: %s\n" % self.name)

    def log(self, line):
        with open(self.filename(), "a", encoding="utf8") as file:
            file.write(line + "\n")

    def check(self):
        with open(self.filename(), encoding="utf8") as file:
            with open(self.filename() + ".expected", encoding="utf8") as file_expected:
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
    mocker.patch("os.getcwd", return_value="someworkdir")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    gitian_build.build(create_args(mocker))

    log.check()

def test_codesign(mocker):
    log = Log("test_codesign")

    class TemporaryDirectoryMock():

        def __init__(self, dirname):
            self.dirname = dirname

        def __enter__(self):
            return self.dirname

        def __exit__(self, *args):
            pass

    original_get_signatures_path = gitian_build.get_signatures_path
    gitian_build.get_signatures_path = lambda platform_str, ver: Path("version-platform-sigs-path")

    mocker.patch("platform.system", return_value='Darwin')
    mocker.patch("pathlib.Path.mkdir")
    # glob has to return a generator, so ['...', '...'] is not enough
    mocker.patch("pathlib.Path.glob", return_value=(f for f in ['unit-e-someversion-win32-setup-unsigned.exe', 'unit-e-someversion-win32-setup-unsigned.exe']))
    mocker.patch("tempfile.TemporaryDirectory", return_value=TemporaryDirectoryMock("tmp"))
    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    args = create_args(mocker)
    args.osslsigncode_path = 'osslsigncode_executable'
    args.win_code_cert_path = 'somecert'
    args.win_code_key_path = 'somekey'

    gitian_build.codesign(args)

    log.check()
    gitian_build.get_signatures_path = original_get_signatures_path

def test_sign(mocker):
    log = Log("test_sign")

    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("pathlib.Path.is_file", return_value=True)
    original_get_signatures_path = gitian_build.get_signatures_path
    gitian_build.get_signatures_path = lambda platform_str, ver: Path("version-platform-sigs-path")

    args = create_args(mocker)

    args.windows = True
    args.macos = False
    gitian_build.sign(args)

    mocker.patch("pathlib.Path.is_file", return_value=False)
    original_get_signatures_path = gitian_build.get_signatures_path
    gitian_build.get_signatures_path = lambda platform_str, ver: Path("version-platform-sigs-path")

    with raises(SystemExit):
        # Should not find signatures and exit
        gitian_build.sign(args)

    mocker.patch("pathlib.Path.is_file", return_value=True)

    log.check()
    gitian_build.get_signatures_path = original_get_signatures_path

def test_verify(mocker):
    log = Log("test_verify")

    mocker.patch("subprocess.check_call", side_effect=log.log_call)

    mocker.patch("pathlib.Path.is_dir", return_value=True)
    gitian_build.verify(create_args(mocker))

    mocker.patch("subprocess.check_call", side_effect=CalledProcessError(1, 'gverify'))
    with raises(Exception) as e:
        gitian_build.verify(create_args(mocker))
    assert 'Command \'gverify\' returned non-zero exit status 1.' in str(e)

    mocker.patch("pathlib.Path.is_dir", return_value=False)
    with raises(SystemExit):
        gitian_build.verify(create_args(mocker))

    log.check()

def test_setup_linux(mocker):
    log = Log("test_setup_linux")

    mocker.patch("platform.system", return_value="Linux")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)
    mocker.patch("pathlib.Path.is_dir", return_value=False)

    gitian_build.setup(create_args(mocker))

    log.check()

def test_setup_mac(mocker):
    log = Log("test_setup_mac")

    mocker.patch("platform.system", return_value="Darwin")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)
    mocker.patch("pathlib.Path.is_dir", return_value=False)

    mocker.patch("pathlib.Path.exists", return_value=False)
    mock_symlink = mocker.patch("pathlib.Path.symlink_to")

    gitian_build.setup(create_args(mocker))

    assert mock_symlink.call_count == 2
    log.check()

def test_prepare_git_dir(mocker):
    log = Log("test_prepare_git_dir")

    mocker.patch("os.getcwd", return_value="someworkdir")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.check_output", side_effect=log.log_call)

    args = create_args(mocker)
    args.pull = True
    args.skip_checkout = False
    gitian_build.prepare_git_dir(args)

    log.check()

def test_prepare_git_dir_skip(mocker):
    log = Log("test_prepare_git_dir_skip")

    mocker.patch("os.getcwd", return_value="someworkdir")
    mocker.patch("os.chdir", side_effect=log.log_chdir)
    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.check_output", side_effect=log.log_call)

    args = create_args(mocker)
    args.skip_checkout = True
    args.pull = False
    gitian_build.prepare_git_dir(args)

    log.check()

def test_prepare_gitian_descriptors(mocker, tmpdir):
    gitian_build.prepare_gitian_descriptors(source="test_data/gitian_descriptors", target=tmpdir, hosts="a-1 b-2")
    with (tmpdir / "gitian-linux.yml").open() as file:
        assert file.read() == '---\nscript: |\n  HOSTS="a-1 b-2"\n'
    with (tmpdir / "gitian-win.yml").open() as file:
        assert file.read() == '---\nscript: |\n  HOSTS="a-1 b-2"\n'

def test_apt_wrapper(mocker):
    log = Log("test_apt_wrapper")

    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)

    apt = gitian_build.Installer(backend='apt', quiet=False)
    apt.add_requirements('package_1')
    apt.add_requirements('package_2a', 'package_2b')
    apt.try_to_install('package_3')
    apt.updated = False
    apt.batch_install()

    apt = gitian_build.Installer(backend='apt', quiet=True)
    apt.is_installed = lambda p: False
    apt.add_requirements('package_1')
    apt.add_requirements('package_2a', 'package_2b')
    apt.batch_install()
    apt.try_to_install('package_3')

    log.check()

def test_brew_wrapper(mocker):
    log = Log("test_brew_wrapper")

    mocker.patch("subprocess.check_call", side_effect=log.log_call)
    mocker.patch("subprocess.call", side_effect=log.log_call)

    brew = gitian_build.Installer(backend='brew')
    brew.add_requirements('package_1')
    brew.add_requirements('package_2a', 'package_2b')
    brew.try_to_install('package_3')
    brew.updated = False
    brew.batch_install()

    brew = gitian_build.Installer(backend='brew', quiet=True)
    brew.is_installed = lambda p: False
    brew.add_requirements('package_1')
    brew.add_requirements('package_2a', 'package_2b')
    brew.batch_install()
    brew.try_to_install('package_3')

    log.check()

def test_verify_user_specified_osslsigncode(mocker):
    mocker.patch("pathlib.Path.is_file", return_value=False)
    with raises(Exception) as e:
        gitian_build.verify_user_specified_osslsigncode('ossl_path')
    assert 'provided osslsign does not exists: ossl_path' in str(e)

    mocker.patch("pathlib.Path.is_file", return_value=True)
    mocker.patch("subprocess.call", return_value=1)
    with raises(Exception) as e:
        gitian_build.verify_user_specified_osslsigncode('ossl_path')
    assert 'cannot execute provided osslsigncode: ossl_path' in str(e)

    mocker.patch("subprocess.call", side_effect=CalledProcessError(1, 'some cmd'))
    with raises(Exception) as e:
        gitian_build.verify_user_specified_osslsigncode('ossl_path')
    assert 'unexpected exception raised while executing provided osslsigncode: Command \'some cmd\' returned non-zero exit status 1.' in str(e)

    mocker.patch("subprocess.call", return_value=255)
    assert gitian_build.verify_user_specified_osslsigncode('ossl_path') == Path('ossl_path').resolve()

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
    assert original_find_osslsigncode("") is None

    # will find the osslsigncode in osslsigncode-1.7.1/osslsigncode
    mocker.patch("pathlib.Path.is_file", return_value=True)
    assert original_find_osslsigncode("") == Path('osslsigncode-1.7.1/osslsigncode').resolve()


    mocker.patch("subprocess.call", return_value=1)

    # won't find the osslsigncode
    mocker.patch("pathlib.Path.is_file", return_value=False)
    assert original_find_osslsigncode("") is None

    # will find the osslsigncode in osslsigncode-1.7.1/osslsigncode but won't be able to execute it (--version won't return 255)
    mocker.patch("pathlib.Path.is_file", return_value=True)
    assert original_find_osslsigncode("") is None


    # Test that user specified path will be properly returned (sanity test)
    mocker.patch("pathlib.Path.is_file", return_value=True)
    mocker.patch("subprocess.call", return_value=255)
    assert gitian_build.verify_user_specified_osslsigncode('ossl_path') == Path('ossl_path').resolve()
