@webdav
Feature: WebDAV interface

    Scenario: Access the WebDAV root directory with a password
        Given user researcher is authenticated
        When the WebDAV root directory is requested
        Then the WebDAV response status code is "207"
        And the WebDAV response is a well-formed multistatus document
        And the WebDAV response lists collection "public"
        And the WebDAV response lists collection "researcher"

    Scenario Outline: Create a file over WebDAV
        Given user researcher is authenticated
        When data object "<objectname>" is created in WebDAV collection "researcher" with content "Hello WebDAV"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" lists data object "<objectname>"
        And data object "<objectname>" in WebDAV collection "researcher" has content "Hello WebDAV"

        # Test regular object name, as well as one with whitespace
        # and a single quote.
        Examples:
	    | objectname               |
	    | webdav_test_file.txt     |
	    | webdav_test file.txt     |
	    | webdav_test'file.txt     |

    Scenario Outline: Rename a file over WebDAV
        Given user researcher is authenticated
        And a WebDAV test data object "<old_objectname>" exists in collection "researcher"
        When WebDAV data object "researcher/<old_objectname>" is renamed to "researcher/<new_objectname>"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" lists data object "<new_objectname>"
        And WebDAV collection "researcher" does not list data object "<old_objectname>"

        # Test regular object name, as well as one with whitespace
        # and a single quote.
        Examples:
            | old_objectname           | new_objectname               |
            | webdav_test_file.txt     | webdav_test_file_renamed.txt |
            | webdav_test file.txt     | webdav_test file_renamed.txt |
            | webdav_test'file.txt     | webdav_test'file_renamed.txt |

    Scenario Outline: Remove a file over WebDAV
        Given user researcher is authenticated
        And a WebDAV test data object "<objectname>" exists in collection "researcher"
        When WebDAV data object "researcher/<objectname>" is removed
        Then the WebDAV response status code is "204"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" does not list data object "<objectname>"

        # Test regular object name, as well as one with whitespace
        # and a single quote.
        Examples:
            | objectname               |
            | webdav_test_file.txt     |
            | webdav_test file.txt     |
            | webdav_test'file.txt     |


    Scenario Outline: Lock a file over WebDAV
        Given user researcher is authenticated
        And a WebDAV test data object "<objectname>" exists in collection "researcher"
        When WebDAV data object "researcher/<objectname>" is locked
        Then the WebDAV response status code is "200"
        And the WebDAV response is a well-formed lockdiscovery document
        And the WebDAV response includes a lock token
        And WebDAV data object "researcher/<objectname>" reports an active lock

        # Test regular object name, as well as one with whitespace
        # and a single quote.
        Examples:
            | objectname               |
            | webdav_test_file.txt     |
            | webdav_test file.txt     |
            | webdav_test'file.txt     |

    Scenario Outline: Unlock a file over WebDAV
        Given user researcher is authenticated
        And a WebDAV test data object "<objectname>" exists in collection "researcher"
        And the WebDAV data object "researcher/<objectname>" is locked
        When the WebDAV data object "researcher/<objectname>" lock is released
        Then the WebDAV response status code is "204"
        And WebDAV data object "researcher/<objectname>" reports no active lock

        # Test regular object name, as well as one with whitespace
        # and a single quote.
        Examples:
            | objectname               |
            | webdav_test_file.txt     |
            | webdav_test file.txt     |
            | webdav_test'file.txt     |

    Scenario: Create a directory over WebDAV
        Given user researcher is authenticated
        When collection "<collection>" is created in WebDAV collection "researcher"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" lists subcollection "<collection>"

	# Test regular collection name, as well as one with whitespace
        # and a single quote.
        Examples:
            | collection               |
            | webdav_test_dir          |
            | webdav_test dir          |
            | webdav_test'dir          |

    Scenario Outline: Rename a directory over WebDAV
        Given user researcher is authenticated
        And a WebDAV test collection "<old_collection>" exists in collection "researcher"
        When WebDAV collection "researcher/<old_collection>" is renamed to "researcher/<new_collection>"
        Then the WebDAV response status code is "201"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" lists subcollection "<new_collection>"
        And WebDAV collection "researcher" does not list subcollection "<old_collection>"

        # Test regular collection name, as well as one with whitespace
        # and a single quote.
        Examples:
            | old_collection           | new_collection          |
            | webdav_test_dir          | webdav_test_dir_renamed |
            | webdav_test dir          | webdav_test dir_renamed |
            | webdav_test'dir          | webdav_test'dir_renamed |

    Scenario: Remove a directory over WebDAV
        Given user researcher is authenticated
        And a WebDAV test collection "<collection>" exists in collection "researcher"
        When WebDAV collection "researcher/<collection>" is removed
        Then the WebDAV response status code is "204"
        And the WebDAV response body is empty or a well-formed multistatus document
        And WebDAV collection "researcher" does not list subcollection "<collection>"

	# Test regular collection name, as well as one with whitespace
        # and a single quote.
        Examples:
            | collection               |
            | webdav_test_dir          |
            | webdav_test dir          |
            | webdav_test'dir          |

    Scenario Outline: Reject WebDAV operations performed with an invalid password
        Given user researcher is authenticated
        When a WebDAV "<method>" request for "<path>" is made with an invalid password
        Then the WebDAV response status code is "401"

        Examples:
            | method   | path                            |
            | PROPFIND | researcher                      |
            | GET      | researcher/testdata/lorem.txt   |
            | PUT      | researcher/wrong_creds_file.txt |
            | PUT      | researcher/wrong_creds file.txt |
            | PUT      | researcher/wrong_creds'file.txt |
            | MKCOL    | researcher/wrong_creds_dir      |
            | MKCOL    | researcher/wrong_creds dir      |
            | MKCOL    | researcher/wrong_creds'dir      |
            | DELETE   | researcher/testdata/lorem.txt   |

    Scenario Outline: Reject WebDAV operations performed with a nonexistent user name
        When a WebDAV "<method>" request for "<path>" is made with a nonexistent user name
        Then the WebDAV response status code is "401"

        Examples:
            | method   | path                            |
            | PROPFIND | researcher                      |
            | GET      | researcher/testdata/lorem.txt   |
            | PUT      | researcher/wrong_creds_file.txt |
            | PUT      | researcher/wrong_creds file.txt |
            | PUT      | researcher/wrong_creds'file.txt |
            | MKCOL    | researcher/wrong_creds_dir      |
            | MKCOL    | researcher/wrong_creds dir      |
            | MKCOL    | researcher/wrong_creds'dir      |
            | DELETE   | researcher/testdata/lorem.txt   |

    Scenario Outline: Reject WebDAV writes to collection without write permissions
        Given user researcher is authenticated
        When a WebDAV "<method>" request for "<path>" is made
        Then the WebDAV response status code is "403"

        Examples:
            | method | path              |
            | PUT    | /webdav_test.txt  |


    Scenario Outline: Reject WebDAV directory creation in collection without write permissions
        Given user researcher is authenticated
        When a WebDAV "<method>" request for "<path>" is made
        # 403 would make more sense than 500 from a protocol point
        # of view, but this documents current behaviour.
        Then the WebDAV response status code is "500"

        Examples:
            | method | path              |
            | MKCOL  | /webdav_test_dir  |


    Scenario: A WebDAV lock prevents modification without the lock token
        Given user researcher is authenticated
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "researcher"
        And the WebDAV data object "researcher/webdav_test_file.txt" is locked
        When data object "webdav_test_file.txt" is created in WebDAV collection "researcher" with content "overwrite attempt"
        Then the WebDAV response status code is "423"

    Scenario: A WebDAV lock allows modification with the lock token
        Given user researcher is authenticated
        And a WebDAV test data object "webdav_test_file.txt" exists in collection "researcher"
        And the WebDAV data object "researcher/webdav_test_file.txt" is locked
        When WebDAV data object "researcher/webdav_test_file.txt" is overwritten with content "new content" using the lock token
        Then the WebDAV response status code is "204"

    Scenario Outline: Reading a nonexistent WebDAV path returns not found
        Given user researcher is authenticated
        When a WebDAV "<method>" request for "<path>" is made
        Then the WebDAV response status code is "404"

        Examples:
            | method   | path                                      |
            | PROPFIND | researcher/this_does_not_exist            |
            | PROPFIND | researcher/this_does_not_exist space      |
            | PROPFIND | researcher/this_does_not_exist'quote      |
            | GET      | researcher/this_does_not_exist.txt        |
            | GET      | researcher/this_does_not_exist space.txt  |
            | GET      | researcher/this_does_not_exist'quote.txt  |

    Scenario: Creating a WebDAV collection with a missing parent fails
        Given user researcher is authenticated
        When a WebDAV "MKCOL" request for "researcher/nonexistent_parent/child" is made
        Then the WebDAV response status code is "409"
