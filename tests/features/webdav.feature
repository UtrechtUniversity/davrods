@webdav
Feature: WebDAV interface

    Scenario: Access the WebDAV root directory with a data access token
        Given user researcher is authenticated
        And a data access token is generated for user researcher
        When the WebDAV root directory is requested
        Then the WebDAV response status code is "207"
        And the WebDAV response is a well-formed multistatus document
        And the WebDAV response lists collection "/tempZone/home"

    # Scenario Outline: Read an existing data object over WebDAV - Empty directory by default
    #     Given user researcher is authenticated
    #     And a data access token is generated for user researcher
    #     When data object "<file>" in WebDAV collection "research-initial/testdata" is requested
    #     Then the WebDAV data object response status code is "200"
    #     And the WebDAV data object is listed in its parent collection
    #     And the size of the WebDAV data object matches the size in the directory listing

    #     Examples:
    #         | file           |
    #         | creatures.json |
    #         | lorem.txt      |
    #         | image.txt      |

    Scenario: Create a file over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        When data object "webdav_test_file.txt" is created in WebDAV collection "home" with content "Hello WebDAV"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "home" lists data object "webdav_test_file.txt"
        And data object "webdav_test_file.txt" in WebDAV collection "home" has content "Hello WebDAV"

    Scenario: Rename a file over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        When WebDAV data object "webdav_test_file.txt" is renamed to "webdav_test_file_renamed.txt"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "home" lists data object "webdav_test_file_renamed.txt"
        And WebDAV collection "home" does not list data object "webdav_test_file.txt"

    Scenario: Remove a file over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        When WebDAV data object "webdav_test_file.txt" is removed
        Then the WebDAV response status code is "204"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "/tempZone/home" does not list data object "webdav_test_file.txt"

    Scenario: Lock a file over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        When WebDAV data object "webdav_test_file.txt" is locked
        Then the WebDAV response status code is "200"
        And the WebDAV response is a well-formed lockdiscovery document
        And the WebDAV response includes a lock token
        And WebDAV data object "webdav_test_file.txt" reports an active lock

    Scenario: Unlock a file over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        And the WebDAV data object "webdav_test_file.txt" is locked
        When the WebDAV data object "webdav_test_file.txt" lock is released
        Then the WebDAV response status code is "204"
        And WebDAV data object "webdav_test_file.txt" reports no active lock

    Scenario: Create a directory over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        When collection "webdav_test_dir" is created in WebDAV collection "home"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "/tempZone/home" lists subcollection "webdav_test_dir"

    Scenario: Rename a directory over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home/research-initial is unlocked
        And a WebDAV test collection "webdav_test_dir" exists in collection "home"
        When WebDAV collection "webdav_test_dir" is renamed to "webdav_test_dir_renamed"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "/tempZone/home" lists subcollection "webdav_test_dir_renamed"
        And WebDAV collection "/tempZone/home" does not list subcollection "webdav_test_dir"

    Scenario: Remove a directory over WebDAV
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home/research-initial is unlocked
        And a WebDAV test collection "webdav_test_dir" exists in collection "home"
        When WebDAV collection "webdav_test_dir" is removed
        Then the WebDAV response status code is "204"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "/tempZone/home" does not list subcollection "webdav_test_dir"

    # Scenario Outline: Reject WebDAV operations performed with an invalid data access token
    #     Given user researcher is authenticated
    #     When a WebDAV "<method>" request for "<path>" is made with an invalid data access token
    #     Then the WebDAV response status code is "401"

    #     Examples:
    #         | method   | path                                  |
    #         | PROPFIND | research-initial                      |
    #         | GET      | research-initial/testdata/lorem.txt   |
    #         | PUT      | research-initial/wrong_creds_file.txt |
    #         | MKCOL    | research-initial/wrong_creds_dir      |
    #         | DELETE   | research-initial/testdata/lorem.txt   |

    Scenario Outline: Reject WebDAV operations performed with a nonexistent user name
        When a WebDAV "<method>" request for "<path>" is made with a nonexistent user name
	Then the WebDAV response status code is "401"

        Examples:
            | method   | path                                  |
            | PROPFIND | /tempZone/home                      |
            | GET      | /tempZone/home/testdata/lorem.txt   |
            | PUT      | /tempZone/home/wrong_creds_file.txt |
            | MKCOL    | /tempZone/home/wrong_creds_dir      |
            | DELETE   | /tempZone/home/testdata/lorem.txt   |

    Scenario Outline: Reject WebDAV writes to a locked research folder
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And research collection "/tempZone/home" is locked during the test
        When a WebDAV "<method>" request for "<path>" is made
        # Code 500 reflects current behaviour. 403 would perhaps
        # make more sense from a protocol perspective, but that is
        # not how it works now.
	Then the WebDAV response status code is "500"

        Examples:
            | method | path                                |
            | MKCOL  | /tempZone/home/locked_test_dir    |
            | PUT    | /tempZone/home/locked_test.txt    |
            | DELETE | /tempZone/home/testdata/lorem.txt |

    # Scenario Outline: Reject WebDAV writes to the vault for file
    #     Given user researcher is authenticated
    #     And a data access token is generated for user researcher
    #     When a WebDAV "<method>" request for "<path>" is made
    #     Then the WebDAV response status code is "403"
    #     Examples:
    #         | method | path                             |
    #         | PUT    | vault-default-1/webdav_test.txt  |


    # Scenario Outline: Reject WebDAV writes to the vault for directory
    #     Given user researcher is authenticated
    #     And a data access token is generated for user researcher
    #     When a WebDAV "<method>" request for "<path>" is made
    #     # code 409 if the researcher does not have access to the
    #     # top-level collection; 500 otherwise.
    #     # 403 would make more sense than 500 from a protocol point
    #     # of view, but this documents current behaviour.
    #     Then the WebDAV response status code is one of "409,500"
    #     Examples:
    #         | method | path                             |
    #         | MKCOL  | vault-default-1/webdav_test_dir  |


    Scenario: A WebDAV lock prevents modification without the lock token
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        And the WebDAV data object "webdav_test_file.txt" is locked
        When data object "webdav_test_file.txt" is created in WebDAV collection "home" with content "overwrite attempt"
        Then the WebDAV response status code is "423"

    Scenario: A WebDAV lock allows modification with the lock token
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "home"
        And the WebDAV data object "webdav_test_file.txt" is locked
        When WebDAV data object "webdav_test_file.txt" is overwritten with content "new content" using the lock token
        Then the WebDAV response status code is "204"

    Scenario Outline: Reading a nonexistent WebDAV path returns not found
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        When a WebDAV "<method>" request for "<path>" is made
        Then the WebDAV response status code is "404"

        Examples:
            | method   | path                                      |
            | PROPFIND | /tempZone/home/this_does_not_exist      |
            | GET      | /tempZone/home/this_does_not_exist.txt  |

    Scenario: Creating a WebDAV collection with a missing parent fails
        Given user researcher is authenticated
        # And a data access token is generated for user researcher - User is logged in?
        And /tempZone/home is unlocked
        When a WebDAV "MKCOL" request for "/tempZone/home/nonexistent_parent/child" is made
    #     Then the WebDAV response status code is "409"
