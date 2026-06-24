# coding=utf-8
"""WebDAV interface feature tests."""

__copyright__ = 'Copyright (c) 2025, Utrecht University'
__license__   = 'GPLv3, see LICENSE'

import urllib.parse
from xml.etree import ElementTree

import pytest
import requests
import urllib3
from pytest_bdd import (
    given,
    parsers,
    scenarios,
    then,
    when,
)

from conftest import api_request,configuration, roles

scenarios('../features/webdav.feature')

# WebDAV elements live in the "DAV:" XML namespace.
DAV_NS = "DAV:"


def _dav(name):
    """Return a namespace-qualified WebDAV element tag, e.g. {DAV:}multistatus."""
    return "{%s}%s" % (DAV_NS, name)


def parse_multistatus(response):
    """Validate that a PROPFIND response is a well-formed WebDAV multistatus
    document and return the set of collection (directory) names it lists.

    The requested root collection itself is excluded from the result, so the
    returned set contains only the subdirectories.

    :param response: requests.Response of a PROPFIND request

    :returns: set of collection names found in the response
    """
    # Parsing raises ElementTree.ParseError if the body is not well-formed XML.
    root = ElementTree.fromstring(response.content)

    assert root.tag == _dav("multistatus"), \
        "Root element is {}, expected {}".format(root.tag, _dav("multistatus"))

    responses = root.findall(_dav("response"))
    assert responses, "multistatus document contains no <response> elements"

    names = set()
    for resp in responses:
        hrefs = resp.findall(_dav("href"))
        assert len(hrefs) == 1 and hrefs[0].text, \
            "<response> must contain exactly one non-empty <href>"
        href = hrefs[0].text

        propstats = resp.findall(_dav("propstat"))
        if resp.find(_dav("status")) is None:
            assert propstats, "<response> without <status> or <propstat>"
            for propstat in propstats:
                assert propstat.find(_dav("status")) is not None, \
                    "<propstat> without <status>"

        # An entry is a directory when its resourcetype contains <collection>.
        is_collection = any(
            propstat.find(_dav("prop")) is not None
            and propstat.find(_dav("prop")).find(_dav("resourcetype")) is not None
            and propstat.find(_dav("prop")).find(_dav("resourcetype")).find(_dav("collection")) is not None
            for propstat in propstats
        )

        if is_collection:
            name = urllib.parse.unquote(href).rstrip("/").rsplit("/", 1)[-1]
            if name:
                names.add(name)

    return names


def parse_content_lengths(response):
    """Parse a PROPFIND multistatus response and return a mapping of entry name
    to its advertised size (the DAV:getcontentlength property).

    Only data objects (non-collections) advertise a content length, so
    collections are naturally absent from the result.

    :param response: requests.Response of a PROPFIND request

    :returns: dict mapping entry name to its size in bytes (int)
    """
    root = ElementTree.fromstring(response.content)

    assert root.tag == _dav("multistatus"), \
        "Root element is {}, expected {}".format(root.tag, _dav("multistatus"))

    sizes = {}
    for resp in root.findall(_dav("response")):
        href = resp.findtext(_dav("href"))
        assert href, "<response> must contain a non-empty <href>"
        name = urllib.parse.unquote(href).rstrip("/").rsplit("/", 1)[-1]

        for propstat in resp.findall(_dav("propstat")):
            prop = propstat.find(_dav("prop"))
            if prop is None:
                continue
            length = prop.find(_dav("getcontentlength"))
            if length is not None and length.text is not None and length.text != "":
                sizes[name] = int(length.text)

    return sizes


def webdav_url():
    """Return the base WebDAV (davrods) URL for the current environment."""
    return configuration.get("davrods_url", "https://data.davrods/")


@pytest.fixture(scope="session")
def data_access_token_cache():
    """Cache of data access tokens per user, shared across the whole test session.

    Generating a token is an API round-trip, so we do it only once per user and
    reuse the result in every scenario.

    :returns: data access token cache
    """
    return {}


@given(parsers.parse("a data access token is generated for user {user}"), target_fixture="data_access_token")
def data_access_token_generate(user, data_access_token_cache):
    # Generate a token only the first time it is requested for a user; subsequent
    # scenarios reuse the cached token.
    if user not in data_access_token_cache:
        # Make sure a rerun starts clean: a token label is unique per user,
        # so remove any leftover token from a previous run before generating.
        api_request(user, "token_delete", {"label": "webdav_test_token"})

        http_status, body = api_request(
            user,
            "token_generate",
            {"label": "webdav_test_token"}
        )
        assert http_status == 200

        token = body["data"]
        assert token
        data_access_token_cache[user] = token

    return data_access_token_cache[user]


@pytest.fixture
def webdav_session(request):
    """A requests.Session reused for every WebDAV request within a scenario.

    Reusing a single connection lets davrods keep the authenticated iRODS
    connection alive (HTTP keep-alive) instead of re-authenticating on every
    request, which is the dominant per-request cost.

    The user name and data access token are resolved here rather than declared
    as fixture parameters because the token is produced by a Given step; it is
    available by the time the first request is made (when this fixture is first
    set up).

    :param request: the request

    :yields:        session
    """
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

    user = request.getfixturevalue("user")
    data_access_token = request.getfixturevalue("data_access_token")

    session = requests.Session()
    session.auth = (roles[user]["username"], data_access_token)
    session.verify = False

    yield session

    session.close()


@when("the WebDAV root directory is requested", target_fixture="webdav_response")
def webdav_request_root(webdav_session):
    # Authenticate to WebDAV (davrods) with the user name and the data access
    # token as password, and list the root directory with a PROPFIND request.
    return webdav_session.request(
        "PROPFIND",
        webdav_url() + "/",
        headers={"Depth": "1"},
        timeout=60,
    )


@when(
    parsers.parse('a WebDAV "{method}" request for "{path}" is made with an invalid data access token'),
    target_fixture="webdav_response",
)
def webdav_request_invalid_token(user, method, path):
    # Use the real user name but an invalid data access token as the password.
    # davrods authenticates HTTP basic credentials against iRODS, so a bad token
    # is rejected with 401 before the WebDAV method itself is processed - this
    # holds for both read (PROPFIND, GET) and write (PUT, MKCOL, DELETE) methods.
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    url = webdav_url() + "/" + urllib.parse.quote(path.strip("/"))
    return requests.request(
        method,
        url,
        auth=(roles[user]["username"], "invalid-data-access-token"),
        verify=False,
        timeout=60,
    )


@when(
    parsers.parse('a WebDAV "{method}" request for "{path}" is made with a nonexistent user name'),
    target_fixture="webdav_response",
)
def webdav_request_nonexistent_user(method, path):
    # davrods authenticates HTTP basic credentials against iRODS, so an unknown
    # user name is rejected with 401 before the WebDAV method itself is
    # processed - this holds for both read and write methods.
    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    url = webdav_url() + "/" + urllib.parse.quote(path.strip("/"))
    return requests.request(
        method,
        url,
        auth=("nonexistent-webdav-user", "some-password"),
        verify=False,
        timeout=60,
    )


@then(parsers.parse('the WebDAV response status code is "{code:d}"'))
def webdav_response_code(webdav_response, code):
    assert webdav_response.status_code == code


@then(parsers.parse('the WebDAV response status code is one of "{code}"'))
def webdav_response_code_multiple(webdav_response, code):
    assert str(webdav_response.status_code) in code.split(",")


@then("the WebDAV response is a well-formed multistatus document")
def webdav_response_well_formed(webdav_response):
    assert webdav_response.status_code == 207
    content_type = webdav_response.headers.get("Content-Type", "")
    assert content_type.startswith(("application/xml", "text/xml")), \
        "Unexpected Content-Type: {}".format(content_type)
    parse_multistatus(webdav_response)


@then(parsers.parse('the WebDAV response lists collection "{name}"'))
def webdav_response_lists_collection(webdav_response, name):
    names = parse_multistatus(webdav_response)
    assert name in names, \
        "Collection '{}' not found. Collections listed: {}".format(name, sorted(names))


@when(
    parsers.parse('data object "{name}" in WebDAV collection "{collection}" is requested'),
    target_fixture="webdav_file",
)
def webdav_request_file(webdav_session, name, collection):
    collection_url = webdav_url() + "/" + collection.strip("/") + "/"
    file_url = collection_url + urllib.parse.quote(name)

    # List the parent collection so we know the size davrods advertises for the
    # data object in the directory listing (the DAV:getcontentlength property).
    listing = webdav_session.request(
        "PROPFIND",
        collection_url,
        headers={"Depth": "1"},
        timeout=60)

    get_response = webdav_session.get(file_url, timeout=60)

    return {
        "name": name,
        "listing": listing,
        "get_response": get_response,
    }


def webdav_collection_url(path):
    return webdav_url() + "/" + path.strip("/") + "/"


def list_subcollections(webdav_session, path):
    response = webdav_session.request(
        "PROPFIND",
        webdav_collection_url(path),
        headers={"Depth": "1"},
        timeout=60,
    )
    assert response.status_code == 207, \
        "PROPFIND on '{}' returned {}".format(path, response.status_code)
    return parse_multistatus(response)


@pytest.fixture
def webdav_cleanup_paths(request):
    paths = set()
    yield paths

    if not paths:
        return

    # Resolve these only at teardown: data_access_token is produced by a step during
    # the scenario, so it is not available when this fixture is first set up.
    user = request.getfixturevalue("user")
    data_access_token = request.getfixturevalue("data_access_token")

    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    with requests.Session() as session:
        session.auth = (roles[user]["username"], data_access_token)
        session.verify = False
        for url in paths:
            try:
                session.request("DELETE", url, timeout=60)
            except requests.RequestException:
                pass


@given(parsers.parse('a WebDAV test collection "{name}" exists in collection "{parent}"'))
def webdav_test_collection_exists(webdav_session, webdav_cleanup_paths, name, parent):
    url = webdav_collection_url(parent) + urllib.parse.quote(name)
    webdav_cleanup_paths.add(url)

    response = webdav_session.request("MKCOL", url, timeout=60)
    assert response.status_code == 201, \
        "Setup MKCOL of '{}' returned {}".format(name, response.status_code)


@when(
    parsers.parse('collection "{name}" is created in WebDAV collection "{parent}"'),
    target_fixture="webdav_response",
)
def webdav_create_collection(webdav_session, webdav_cleanup_paths, name, parent):
    url = webdav_collection_url(parent) + urllib.parse.quote(name)
    webdav_cleanup_paths.add(url)

    return webdav_session.request("MKCOL", url, timeout=60)


@when(
    parsers.parse('WebDAV collection "{source}" is renamed to "{destination}"'),
    target_fixture="webdav_response",
)
def webdav_rename_collection(webdav_session, webdav_cleanup_paths, source, destination):
    source_url = webdav_collection_url(source)
    destination_url = webdav_collection_url(destination)
    # Track the destination too, so cleanup removes it regardless of outcome.
    webdav_cleanup_paths.add(destination_url)

    return webdav_session.request(
        "MOVE",
        source_url,
        headers={"Destination": destination_url, "Overwrite": "F"},
        timeout=60,
    )


@when(
    parsers.parse('WebDAV collection "{path}" is removed'),
    target_fixture="webdav_response",
)
def webdav_remove_collection(webdav_session, path):
    return webdav_session.request("DELETE", webdav_collection_url(path), timeout=60)


@then(parsers.parse('the WebDAV data object response status code is "{code:d}"'))
def webdav_file_response_code(webdav_file, code):
    assert webdav_file["get_response"].status_code == code, \
        "Unexpected status code {} for data object '{}'".format(
            webdav_file["get_response"].status_code, webdav_file["name"])


@then("the WebDAV data object is listed in its parent collection")
def webdav_file_listed(webdav_file):
    assert webdav_file["listing"].status_code == 207, \
        "PROPFIND on parent collection returned {}".format(webdav_file["listing"].status_code)
    sizes = parse_content_lengths(webdav_file["listing"])
    assert webdav_file["name"] in sizes, \
        "Data object '{}' not found in directory listing. Listed: {}".format(
            webdav_file["name"], sorted(sizes))


@then("the size of the WebDAV data object matches the size in the directory listing")
def webdav_file_size_matches(webdav_file):
    get_response = webdav_file["get_response"]
    sizes = parse_content_lengths(webdav_file["listing"])
    listed_size = sizes[webdav_file["name"]]

    body_size = len(get_response.content)
    assert body_size == listed_size, \
        "Downloaded size {} does not match listed size {} for '{}'".format(
            body_size, listed_size, webdav_file["name"])

    # When present, the Content-Length response header should agree as well.
    header_length = get_response.headers.get("Content-Length")
    if header_length is not None:
        assert int(header_length) == listed_size, \
            "Content-Length header {} does not match listed size {} for '{}'".format(
                header_length, listed_size, webdav_file["name"])


@then("the WebDAV response body is empty or a well-formed multistatus document")
def webdav_response_optional_multistatus(webdav_response):
    body = webdav_response.content
    # A successful MKCOL/MOVE/DELETE usually has no body; only validate when the
    # server does return an (XML) body, in which case it must be a multistatus.
    if not body.strip():
        return
    content_type = webdav_response.headers.get("Content-Type", "")
    if content_type.startswith(("application/xml", "text/xml")):
        parse_multistatus(webdav_response)


@then(parsers.parse('WebDAV collection "{parent}" lists subcollection "{name}"'))
def webdav_lists_subcollection(webdav_session, parent, name):
    names = list_subcollections(webdav_session, parent)
    assert name in names, \
        "Subcollection '{}' not found in '{}'. Listed: {}".format(name, parent, sorted(names))


@then(parsers.parse('WebDAV collection "{parent}" does not list subcollection "{name}"'))
def webdav_not_lists_subcollection(webdav_session, parent, name):
    names = list_subcollections(webdav_session, parent)
    assert name not in names, \
        "Subcollection '{}' unexpectedly present in '{}'. Listed: {}".format(name, parent, sorted(names))


def webdav_object_url(path):
    """Return the absolute WebDAV URL for a data object path (no trailing slash)."""
    # quote() keeps "/" by default, so path separators are preserved.
    return webdav_url() + "/" + urllib.parse.quote(path.strip("/"))


def list_data_objects(webdav_session, path):
    response = webdav_session.request(
        "PROPFIND",
        webdav_collection_url(path),
        headers={"Depth": "1"},
        timeout=60,
    )
    assert response.status_code == 207, \
        "PROPFIND on '{}' returned {}".format(path, response.status_code)
    return set(parse_content_lengths(response).keys())


@given(parsers.parse('a WebDAV test data object "{name}" exists in collection "{parent}"'))
def webdav_test_data_object_exists(webdav_session, webdav_cleanup_paths, name, parent):
    url = webdav_collection_url(parent) + urllib.parse.quote(name)
    webdav_cleanup_paths.add(url)

    response = webdav_session.request("PUT", url, data=b"test data", timeout=60)
    assert response.status_code == 201, \
        "Setup PUT of '{}' returned {}".format(name, response.status_code)


@when(
    parsers.parse('data object "{name}" is created in WebDAV collection "{parent}" with content "{content}"'),
    target_fixture="webdav_response",
)
def webdav_create_data_object(webdav_session, webdav_cleanup_paths, name, parent, content):
    url = webdav_collection_url(parent) + urllib.parse.quote(name)
    webdav_cleanup_paths.add(url)

    return webdav_session.request("PUT", url, data=content.encode("utf-8"), timeout=60)


@when(
    parsers.parse('WebDAV data object "{source}" is renamed to "{destination}"'),
    target_fixture="webdav_response",
)
def webdav_rename_data_object(webdav_session, webdav_cleanup_paths, source, destination):
    destination_url = webdav_object_url(destination)
    # Track the destination too, so cleanup removes it regardless of outcome.
    webdav_cleanup_paths.add(destination_url)

    return webdav_session.request(
        "MOVE",
        webdav_object_url(source),
        headers={"Destination": destination_url, "Overwrite": "F"},
        timeout=60,
    )


@when(
    parsers.parse('WebDAV data object "{path}" is removed'),
    target_fixture="webdav_response",
)
def webdav_remove_data_object(webdav_session, path):
    return webdav_session.request("DELETE", webdav_object_url(path), timeout=60)


@then(parsers.parse('WebDAV collection "{parent}" lists data object "{name}"'))
def webdav_lists_data_object(webdav_session, parent, name):
    names = list_data_objects(webdav_session, parent)
    assert name in names, \
        "Data object '{}' not found in '{}'. Listed: {}".format(name, parent, sorted(names))


@then(parsers.parse('WebDAV collection "{parent}" does not list data object "{name}"'))
def webdav_not_lists_data_object(webdav_session, parent, name):
    names = list_data_objects(webdav_session, parent)
    assert name not in names, \
        "Data object '{}' unexpectedly present in '{}'. Listed: {}".format(name, parent, sorted(names))


@then(parsers.parse('data object "{name}" in WebDAV collection "{parent}" has content "{content}"'))
def webdav_data_object_has_content(webdav_session, parent, name, content):
    url = webdav_collection_url(parent) + urllib.parse.quote(name)
    response = webdav_session.get(url, timeout=60)
    assert response.status_code == 200, \
        "GET of '{}' returned {}".format(name, response.status_code)
    assert response.text == content, \
        "Content of '{}' is {!r}, expected {!r}".format(name, response.text, content)


# Body sent with a LOCK request: an exclusive write lock owned by the test suite.
LOCKINFO_BODY = (
    '<?xml version="1.0" encoding="utf-8"?>\n'
    '<D:lockinfo xmlns:D="DAV:">\n'
    '  <D:lockscope><D:exclusive/></D:lockscope>\n'
    '  <D:locktype><D:write/></D:locktype>\n'
    '  <D:owner><D:href>webdav-testsuite</D:href></D:owner>\n'
    '</D:lockinfo>'
)


def parse_lock_token(response):
    """Validate that a LOCK response is a well-formed WebDAV lockdiscovery
    document and return the lock token it advertises.

    The expected shape is a DAV:prop document containing
    lockdiscovery > activelock > {lockscope, locktype, locktoken > href}.

    :param response: requests.Response of a LOCK request

    :returns: the lock token (the text of the locktoken's href element)
    """
    root = ElementTree.fromstring(response.content)

    assert root.tag == _dav("prop"), \
        "Root element is {}, expected {}".format(root.tag, _dav("prop"))

    lockdiscovery = root.find(_dav("lockdiscovery"))
    assert lockdiscovery is not None, "<lockdiscovery> element missing"

    activelock = lockdiscovery.find(_dav("activelock"))
    assert activelock is not None, "<activelock> element missing"
    assert activelock.find(_dav("lockscope")) is not None, "<lockscope> missing"
    assert activelock.find(_dav("locktype")) is not None, "<locktype> missing"

    locktoken = activelock.find(_dav("locktoken"))
    assert locktoken is not None, "<locktoken> element missing"
    href = locktoken.find(_dav("href"))
    assert href is not None and href.text, "<locktoken> without a non-empty <href>"

    return href.text.strip()


def send_lock(webdav_session, path):
    """Issue a LOCK request for a data object and return the response."""
    return webdav_session.request(
        "LOCK",
        webdav_object_url(path),
        data=LOCKINFO_BODY.encode("utf-8"),
        headers={"Timeout": "Second-300", "Depth": "0", "Content-Type": "application/xml"},
        timeout=60,
    )


def get_active_lock_tokens(webdav_session, path):
    body = (
        '<?xml version="1.0" encoding="utf-8"?>\n'
        '<D:propfind xmlns:D="DAV:"><D:prop><D:lockdiscovery/></D:prop></D:propfind>'
    )
    response = webdav_session.request(
        "PROPFIND",
        webdav_object_url(path),
        data=body.encode("utf-8"),
        headers={"Depth": "0", "Content-Type": "application/xml"},
        timeout=60,
    )
    assert response.status_code == 207, \
        "PROPFIND on '{}' returned {}".format(path, response.status_code)

    root = ElementTree.fromstring(response.content)
    tokens = set()
    for locktoken in root.iter(_dav("locktoken")):
        href = locktoken.find(_dav("href"))
        if href is not None and href.text:
            tokens.add(href.text.strip())
    return tokens


@pytest.fixture
def webdav_locks(request, webdav_cleanup_paths):
    locks = []
    yield locks

    if not locks:
        return

    user = request.getfixturevalue("user")
    data_access_token = request.getfixturevalue("data_access_token")

    urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
    with requests.Session() as session:
        session.auth = (roles[user]["username"], data_access_token)
        session.verify = False
        for url, token_header in locks:
            try:
                session.request(
                    "UNLOCK",
                    url,
                    headers={"Lock-Token": token_header},
                    timeout=60,
                )
            except requests.RequestException:
                pass


@when(
    parsers.parse('WebDAV data object "{path}" is locked'),
    target_fixture="webdav_response",
)
def webdav_lock_data_object(webdav_session, webdav_locks, path):
    response = send_lock(webdav_session, path)
    token_header = response.headers.get("Lock-Token")
    if token_header:
        webdav_locks.append((webdav_object_url(path), token_header))
    return response


@given(
    parsers.parse('the WebDAV data object "{path}" is locked'),
    target_fixture="webdav_lock_token",
)
def webdav_data_object_is_locked(webdav_session, webdav_locks, path):
    response = send_lock(webdav_session, path)
    assert response.status_code == 200, \
        "Setup LOCK of '{}' returned {}".format(path, response.status_code)
    token_header = response.headers.get("Lock-Token")
    assert token_header, "LOCK response for '{}' has no Lock-Token header".format(path)
    webdav_locks.append((webdav_object_url(path), token_header))
    return token_header


@when(
    parsers.parse('the WebDAV data object "{path}" lock is released'),
    target_fixture="webdav_response",
)
def webdav_unlock_data_object(webdav_session, webdav_lock_token, path):
    return webdav_session.request(
        "UNLOCK",
        webdav_object_url(path),
        headers={"Lock-Token": webdav_lock_token},
        timeout=60,
    )


@then("the WebDAV response is a well-formed lockdiscovery document")
def webdav_response_lockdiscovery(webdav_response):
    content_type = webdav_response.headers.get("Content-Type", "")
    assert content_type.startswith(("application/xml", "text/xml")), \
        "Unexpected Content-Type: {}".format(content_type)
    parse_lock_token(webdav_response)


@then("the WebDAV response includes a lock token")
def webdav_response_includes_lock_token(webdav_response):
    header = webdav_response.headers.get("Lock-Token")
    assert header, "Lock-Token response header missing"
    body_token = parse_lock_token(webdav_response)
    assert body_token in header, \
        "Lock-Token header {!r} does not match body token {!r}".format(header, body_token)


@then(parsers.parse('WebDAV data object "{path}" reports an active lock'))
def webdav_data_object_reports_lock(webdav_session, path):
    tokens = get_active_lock_tokens(webdav_session, path)
    assert tokens, "Data object '{}' reports no active lock, expected one".format(path)


@then(parsers.parse('WebDAV data object "{path}" reports no active lock'))
def webdav_data_object_reports_no_lock(webdav_session, path):
    tokens = get_active_lock_tokens(webdav_session, path)
    assert not tokens, \
        "Data object '{}' still reports active lock(s): {}".format(path, sorted(tokens))


@given(parsers.parse('research collection "{collection}" is locked during the test'))
def webdav_research_collection_locked(request, user, collection):
    # Lock a Yoda research folder via the API and guarantee it is unlocked again
    # at the end of the scenario, so a leftover lock cannot affect other tests.
    http_status, _ = api_request(user, "folder_lock", {"coll": collection})
    assert http_status == 200, \
        "folder_lock of '{}' returned {}".format(collection, http_status)

    def unlock():
        api_request(user, "folder_unlock", {"coll": collection})

    request.addfinalizer(unlock)


@when(
    parsers.parse('a WebDAV "{method}" request for "{path}" is made'),
    target_fixture="webdav_response",
)
def webdav_request_method(webdav_session, method, path):
    # A request with valid credentials; the scenario asserts the resulting status
    # code (e.g. a write that is rejected by an ACL or lock policy, or a request
    # for a nonexistent path).
    kwargs = {"timeout": 60}
    if method.upper() == "PUT":
        kwargs["data"] = b"test data"
    return webdav_session.request(method, webdav_object_url(path), **kwargs)


@when(
    parsers.parse('WebDAV data object "{path}" is overwritten with content "{content}" using the lock token'),
    target_fixture="webdav_response",
)
def webdav_overwrite_with_lock_token(webdav_session, webdav_lock_token, path, content):
    # Supplying the lock token in the If header should satisfy mod_dav's lock
    # validation, so the write on the locked resource is allowed.
    return webdav_session.request(
        "PUT",
        webdav_object_url(path),
        data=content.encode("utf-8"),
        headers={"If": "({})".format(webdav_lock_token)},
        timeout=60,
    )
