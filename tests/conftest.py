#!/usr/bin/env python3
"""Davrods tests configuration."""

__copyright__ = 'Copyright (c) 2020-2025, Utrecht University'
__license__   = 'GPLv3, see LICENSE'

import json
import re
import sys
from datetime import datetime

import pytest
import requests
import urllib3
from pytest_bdd import (
    given,
    parsers,
    then,
    when,
)

webdav_url = ""
configuration = {}
roles = {}
user_cookies = {}

def pytest_addoption(parser):
    parser.addoption("--webdav", action="store_true", default=False, help="Run WebDAV tests")
    parser.addoption("--environment", action="store", default="environments/docker.json", help="Specify configuration file")


def pytest_configure(config):
    config.addinivalue_line("markers", "webdav: WebDAV test")

    global environment
    environment = config.getoption("--environment")

    # Read environment configuration file.
    global configuration
    with open(environment) as f:
        configuration = json.loads(f.read())

    # Get portal and API url from configuration.
    global url
    url = configuration.get("webdav_url", "https://data.davrods:8445")

    # Get roles from configuration.
    global roles
    roles = configuration.get("roles", {})


    global webdav
    webdav = config.getoption("--webdav")


def pytest_runtest_logreport(report):
    if report.failed and report.when == "call":
        now = datetime.now()
        timestamp = now.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        report.sections.append(("timestamp", f"\n{timestamp} [{report.nodeid}] failed."))


def pytest_bdd_apply_tag(tag, function):
    if tag == "webdav" and not webdav:
        marker = pytest.mark.skip(reason="Skip WebDAV tests")
        marker(function)
        return True
    elif tag == "fail":
        marker = pytest.mark.xfail(reason="Test is expected to fail", run=True, strict=False)
        marker(function)
        return True
    else:
        # Fall back to pytest-bdd's default behavior
        return None


@given(parsers.parse("user {user:w} is authenticated"), target_fixture="user")
def api_user_authenticated(user):
    assert user in roles
    return user
