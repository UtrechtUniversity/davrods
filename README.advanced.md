Advanced Davrods configuration
==============================

The [vhost templates](./aux/rpm/davrods-vhost.conf) installed by the
default Davrods distribution provide for most common Davrods use cases.
Some more interesting configurations are possible using more advanced
Apache features, detailed in this document.

The examples in this document assume Davrods version `4.2.8_1.5.0` or
newer.

## Setting per-user Davrods options ##

[Apache expressions](https://httpd.apache.org/docs/2.4/expr.html) can be
used to set certain Davrods configuration options depending on the HTTP
Basic auth username, such as the iRODS auth scheme, default resource or
exposed root.

Below we provide an example of how to accomplish this:

```apache
<VirtualHost ...>
  ...

  # Parse the Authorization header for a username.
  SetEnvIfExpr "req_novary('Authorization') =~ /^Basic +(.*)/"          DAVRODS_BASIC_CREDS64=$1
  SetEnvIfExpr "unbase64(reqenv('DAVRODS_BASIC_CREDS64')) =~ /(.*:.*)/" DAVRODS_BASIC_CREDS=$1
  SetEnvIfExpr "reqenv('DAVRODS_BASIC_CREDS') =~ /^([^:]*):/"           DAVRODS_USERNAME=$1

  # Davrods directives in these If blocks override directives/defaults
  # in the Location block below it, but only if the condition matches.

  <If "reqenv('DAVRODS_USERNAME') =~ /@/">
    # Use iRODS PAM auth for usernames that look like e-mail addresses.
    DavrodsAuthScheme Pam
  </If>
  <If "reqenv('DAVRODS_USERNAME') == 'rods'">
    # Expose a higher level collection for user rods.
    DavrodsExposedRoot /
  </If>
  ...

  <Location />
    # ... enable Davrods etc. ...

    DavrodsAuthScheme  Native
    DavrodsExposedRoot User
    ...
  </Location>
</VirtualHost>
```

## Enabling iRODS ticket support ##

Davrods optionally allows using
[iRODS tickets](https://docs.irods.org/4.2.8/icommands/tickets/)
to obtain read-only access to collections and data objects.
In order to make use of this feature, special configuration is needed.
Since this configuration is relatively complex and very use-case
dependent, it is not included in the provided
[vhost templates](aux/deb/davrods-vhost.conf).
When no such configuration is present, ticket support is completely
disabled as in previous Davrods versions.
Note: Davrods does not support using tickets for *write* operations
(including creating, deleting, moving or copying files).

WebDAV has no concept of tickets, except for an
[expired IETF draft](https://tools.ietf.org/html/draft-ito-dav-ticket-00)
that no client implements.
There are a few possible methods to still fit iRODS tickets in the
protocol, in a way that is usable for clients:

1. Embedding tickets in query strings (e.g. `?ticket=SECRET` at the end of all URLs)
2. Sending tickets in a custom HTTP header (e.g. `X-Davrods-Ticket: SECRET`)
3. Embedding tickets in Basic authentication (e.g. user=ticket, password=SECRET)

Each method has its own advantages and disadvantages, and none of these
methods work across all WebDAV clients and for every use case.

Davrods makes no assumptions regarding clients and use-cases, and
therefore it leaves it up to the administrator to configure a chosen
method, if they wish to use tickets.

The way this works is as follows:

First of all, ticket support must be enabled by setting
`DavrodsTickets ReadOnly` in the vhost's location block.

Then, using standard Apache config directives such as `SetEnv`, a
request environment variable `DAVRODS_TICKET` can be set. When Davrods
reads this variable, it sends the value as a ticket to iRODS.

The tricky part then, is to use only Apache config directives to extract
the ticket from WebDAV requests. For the three methods described above,
we provide example code that can be inserted into Davrods vhost
configuration files.

As shown in the examples below, the configuration directives should be
placed in the VirtualHost section. They may be combined in a single
vhost by placing them one after the other.

### Using query strings ###

This method makes for good read-only web browser usability and allows
linking directly to ticket-accessible locations. HTML Directory listing
links will have `?ticket=...` appended automatically if a ticket is used
to access a collection in a web browser.

Regular WebDAV clients are unable to use this method.

```apache
<VirtualHost ...>
  ...

  RewriteEngine on

  # If the URL query string contains a ticket, strip it and pass it to Davrods.
  RewriteCond expr "%{QUERY_STRING} =~ /(?:^|&)ticket=([^&]*)/"
  RewriteCond expr "unescape($1) =~ /(.+)/"
  RewriteRule .* - [E=DAVRODS_TICKET:%1,QSD,L]

  <Location />
    ...

    # Enable tickets, and automatically append '?ticket=...' to links in
    # web browser directory listings.
    DavrodsTickets ReadOnly
    DavrodsHtmlEmitTickets On

    ...
  </Location>
</VirtualHost>
```

### Using a custom HTTP header ###

This is not usable in any commonly used WebDAV client or web browser,
but may be used by curl and custom clients.

```apache
<VirtualHost ...>
  ...

  RewriteEngine on

  # If header X-Davrods-Ticket exists, pass it to Davrods.
  RewriteCond "%{HTTP:X-Davrods-Ticket}" "(.+)"
  RewriteRule .* - [E=DAVRODS_TICKET:%1,L]

  <Location />
    ...

    DavrodsTickets ReadOnly

    ...
  </Location>
</VirtualHost>
```

### Using a special-cased Basic auth username ###

The third method is usable in both WebDAV clients and web browsers, but
does not allow combining tickets with a regular iRODS username/password.
It works by switching Davrods to anonymous mode only when a specific
HTTP Basic auth username (e.g. 'ticket') is used.

This involves some trickery, and will not work for anonymous vhosts (as
many clients will not send credentials unless prompted to by the HTTP
server - which means Basic auth needs to be enabled).
We cannot guarantee to support this method in future versions, but it's
here if you need it.

```apache
<VirtualHost ...>
  ...

  # If HTTP basic auth username is "ticket", override Davrods to use
  # anonymous mode for this request and use the provided password as a
  # ticket value.
  # (below you can replace 'ticket:' with whichever username you prefer,
  # followed by a colon. e.g. 'anonymous:')

  SetEnvIfExpr "req_novary('Authorization') =~ /^Basic +(.*)/"          DAVRODS_BASIC_CREDS64=$1
  SetEnvIfExpr "unbase64(reqenv('DAVRODS_BASIC_CREDS64')) =~ /(.*:.*)/" DAVRODS_BASIC_CREDS=$1
  SetEnvIfExpr "reqenv('DAVRODS_BASIC_CREDS') =~ /^ticket:(.*)/"        DAVRODS_BASIC_TICKET=$1
  SetEnvIfExpr "reqenv('DAVRODS_BASIC_TICKET') =~ /^(.*)$/"             DAVRODS_TICKET=$1

  <If "-n reqenv('DAVRODS_BASIC_TICKET')">
    # These settings override settings in the <Location> section below for
    # the current request, but only if the condition matches (i.e. a client
    # has submitted a ticket via Basic auth).
    # This requires Basic auth to be enabled in the Location section
    # below, as most clients will not send an Authorization header
    # unless asked to by the HTTP server.

    # Make sure the Authorization header is ignored by Apache,
    # mod_auth_basic, and Davrods.
    AuthType None
    Require  all granted
    DavrodsAuthScheme Native
    DavrodsAnonymousMode On
    DavrodsAnonymousLogin "anonymous" ""

    # If a ticket is submitted via Basic auth, we should not embed it in HTML listings.
    # (follow-up web browser requests will re-send the ticket via auth anyway)
    DavrodsHtmlEmitTickets Off
  </If>

  <Location />
    ...

    DavrodsTickets ReadOnly

    ...
  </Location>
</VirtualHost>
```
