#!/usr/bin/env python3
"""Yoda tests configuration."""

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

davrods_url = ""
configuration = {}
roles = {}
user_cookies = {}

verbose_test = False


def pytest_addoption(parser):
    parser.addoption("--environment", action="store", default="environments/docker.json", help="Specify configuration file")
    parser.addoption("--verbose-test", action="store_true", default=False, help="Print additional information for troubleshooting purposes")


def pytest_configure(config):
    global environment
    environment = config.getoption("--environment")

    # Read environment configuration file.
    global configuration
    with open(environment) as f:
        configuration = json.loads(f.read())

    # Get Davrods url from configuration.
    global davrods_url
    davrods_url = configuration.get("url", "https://data.davrods")

    # Get roles from configuration.
    global roles
    roles = configuration.get("roles", {})

    global verbose_test
    verbose_test = config.getoption("--verbose-test")


def pytest_runtest_logreport(report):
    if report.failed and report.when == "call":
        now = datetime.now()
        timestamp = now.strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]
        report.sections.append(("timestamp", f"\n{timestamp} [{report.nodeid}] failed."))


def api_request(user, request, data, timeout=60):
    # Retrieve user cookies.
    # csrf, session = user_cookies[user]

    # Disable insecure connection warning.
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

    # Replace zone name with zone name from environment configuration.
    data = json.dumps(data).replace("tempZone", configuration.get("zone_name", "tempZone"))

    # Make API request.
    url = davrods_url + "/" + request
    # files = {'csrf_token': (None, csrf), 'data': (None, data)}
    # cookies = {'__Host-session': session}
    headers = {'referer': davrods_url}
    if verbose_test:
        print("Processing API request for user {} with data {}".format(user, json.dumps(data)))
    response = requests.post(url, headers=headers, verify=False, timeout=timeout)

    # Remove debug info from response body.
    body = response.json()
    if "debug_info" in body:
        del body["debug_info"]

    return (response.status_code, body)


@given(parsers.parse("user {user:w} is authenticated"), target_fixture="user")
def davrods_user_authenticated(user):
    assert user in roles
    print("User authenticated")
    return user

