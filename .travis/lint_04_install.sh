#!/usr/bin/env bash

export LC_ALL=C

travis_retry pip install flake8==3.5.0 pytest-mock
