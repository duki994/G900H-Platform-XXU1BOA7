#!/bin/bash
# Copyright (c) 2013 The Chromium Authors. All rights reserved.
# Use of this source code is governed by a BSD-style license that can be
# found in the LICENSE file.

set -e

http_port=8080
ssh_port=29418

while test $# -ne 0; do
  case "$1" in
    -v)
      version="$2"
      shift
      ;;
    -d)
      rundir="$2"
      shift
      ;;
    --http-port)
      http_port="$2"
      shift
      ;;
    --ssh-port)
      ssh_port="$2"
      shift
      ;;
    *)
      rundir="$1"
      ;;
  esac
  shift
done

if [ -z "$rundir" ]; then
  rundir=$(mktemp -d)
fi

this_dir=$(dirname $0)
gerrit_exe="$this_dir/gerrit.war"

account_id=101
full_name='Test Account'
maximum_page_size='25'
password='test-password'
preferred_email="test-username@test.org"
registered_on=$(date '+%Y-%m-%d %H:%M:%S.000%:::z')
username='test-username'

# The python code below for picking the "latest" gerrit release is cribbed and
# ported from the javascript at:
#
#     http://gerrit-releases.storage.googleapis.com/index.html
url='https://www.googleapis.com/storage/v1beta2/b/gerrit-releases/o?projection=noAcl'
curl --retry 30 --ssl-reqd -s $url | python <(cat <<EOF
# Receives Gerrit version via command line and reads json-encoded
# text from stdin in the format:
#
#    {
#     "items": [
#      {
#       "name": "gerrit-<version>.war",
#       "md5Hash": "<base64 encoded md5sum>",
#      },
#      {
#       "name": "gerrit-<version>.war",
#       "md5Hash": "<base64 encoded md5sum>",
#      },
#      ...
#    }
#
# ...and prints the name and md5sum of the corresponding *.war file.

import json
import re
import sys

requested_version = sys.argv[1] if len(sys.argv) > 1 else None
gerrit_re = re.compile('gerrit(?:-full|-snapshot)?-([0-9.]+)(-rc[0-9]+)?'
                       '(?:-([0-9]*)-g[0-9a-f]+)?[.]war')
j = json.load(sys.stdin)
items = [(x, gerrit_re.match(x['name'])) for x in j['items']]
items = [(x, m.groups()) for x, m in items if m]

def _cmp(a, b):
  a_version, a_rc, a_commits = a[1]
  b_version, b_rc, b_commits = b[1]
  a_parts = a_version.split('.')
  b_parts = b_version.split('.')
  while len(a_parts) < len(b_parts):
    a_parts.append('0')
  while len(b_parts) < len(a_parts):
    b_parts.append('0')
  a_parts.append(a_rc[3:] if a_rc else '1000')
  b_parts.append(b_rc[3:] if b_rc else '1000')
  a_parts.append(a_commits if a_commits else '0')
  b_parts.append(b_commits if b_commits else '0')

  return -cmp(map(int, a_parts), map(int, b_parts))

if requested_version:
  for info, version in items:
    if version == requested_version:
      print '"%s" "%s"' % (info['name'], info['md5Hash'])
      sys.exit(0)
  print >> sys.stderr, 'No such Gerrit version: %s' % requested_version
  sys.exit(1)

items.sort(cmp=_cmp)
for x in items:
  print '"%s" "%s"' % (x[0]['name'], x[0]['md5Hash'])
  sys.exit(0)
EOF
) "$version" | xargs | while read name md5; do
  # Download the requested gerrit version if necessary, and verify the md5sum.
  target="$this_dir/$name"
  net_sum=$(echo -n $md5 | base64 -d | od -tx1 | head -1 | cut -d ' ' -f 2- |
            sed 's/ //g')
  if [ -f "$target" ]; then
    file_sum=$(md5sum "$target" | awk '{print $1}' | xargs)
    if [ "$file_sum" = "$net_sum" ]; then
      ln -sf "$name" "$gerrit_exe"
      break
    else
      rm -rf "$target"
    fi
  fi
  curl --retry 30 --ssl-reqd -s -o "$target" \
      "https://gerrit-releases.storage.googleapis.com/$name"
  file_sum=$(md5sum "$target" | awk '{print $1}' | xargs)
  if [ "$file_sum" != "$net_sum" ]; then
    echo "ERROR: md5sum mismatch when downloading $name" 1>&2
    rm -rf "$target"
    exit 1
  else
    ln -sf "$name" "$gerrit_exe"
  fi
done

if [ ! -e "$gerrit_exe" ]; then
  echo "ERROR: No $gerrit_exe file or link present, and unable " 1>&2
  echo "       to download the latest version." 1>&2
  exit 1
fi

# By default, gerrit only accepts https connections, which is a good thing.  But
# for testing, it's convenient to enable plain http.  Also, turn off all email
# notifications.
mkdir -p "${rundir}/etc"
cat <<EOF > "${rundir}/etc/gerrit.config"
[auth]
	type = DEVELOPMENT_BECOME_ANY_ACCOUNT
	gitBasicAuth = true
[gerrit]
	canonicalWebUrl = http://$(hostname):${http_port}/
[httpd]
	listenUrl = http://*:${http_port}/
[sshd]
	listenAddress = *:${ssh_port}
[sendemail]
	enable = false
[container]
	javaOptions = -Duser.home=${rundir}/tmp
EOF

# Initialize the gerrit instance.
java -jar "$gerrit_exe" init --no-auto-start --batch --install-plugin=download-commands -d "${rundir}"

# Create SSH key pair for the first user.
mkdir -p "${rundir}/tmp"
ssh-keygen -t rsa -q -f "${rundir}/tmp/id_rsa" -N ""
ssh_public_key="$(cat ${rundir}/tmp/id_rsa.pub)"

# Set up the first user, with admin priveleges.
cat <<EOF | java -jar "$gerrit_exe" gsql -d "${rundir}" > /dev/null
INSERT INTO ACCOUNTS (FULL_NAME, MAXIMUM_PAGE_SIZE, PREFERRED_EMAIL, REGISTERED_ON, ACCOUNT_ID) VALUES ('${full_name}', ${maximum_page_size}, '${preferred_email}', '${registered_on}', ${account_id});
INSERT INTO ACCOUNT_EXTERNAL_IDS (ACCOUNT_ID, EXTERNAL_ID) VALUES (${account_id}, 'gerrit:${username}');
INSERT INTO ACCOUNT_EXTERNAL_IDS (ACCOUNT_ID, EXTERNAL_ID) VALUES (${account_id}, 'username:${username}');
INSERT INTO ACCOUNT_EXTERNAL_IDS (ACCOUNT_ID, EMAIL_ADDRESS, PASSWORD) VALUES (${account_id}, '${preferred_email}', '${password}');
INSERT INTO ACCOUNT_GROUP_MEMBERS (ACCOUNT_ID, GROUP_ID) VALUES (${account_id}, 1);
INSERT INTO ACCOUNT_SSH_KEYS (ACCOUNT_ID, SSH_PUBLIC_KEY, VALID, SEQ) VALUES (${account_id}, '${ssh_public_key}', 'Y', 0);
EOF

# Create a netrc file to authenticate as the first user.
cat <<EOF > "${rundir}/tmp/.netrc"
machine localhost login ${username} password ${password}
EOF

# Create a .git-credentials file, to enable password-less push.
cat <<EOF > "${rundir}/tmp/.git-credentials"
http://${username}:${password}@localhost:${http_port}
EOF

cat <<EOF

To start gerrit server:
  ${rundir}/bin/gerrit.sh start

To use the REST API:
  curl --netrc-file ${rundir}/tmp/.netrc http://localhost:${http_port}/<endpoint>

To use SSH API:
  ssh ${username}@localhost -p ${ssh_port} -i ${rundir}/tmp/id_rsa gerrit

To enable 'git push' without a password prompt:
  git config credential.helper 'store --file=${rundir}/tmp/.git-credentials'

To stop the server:
  ${rundir}/bin/gerrit.sh stop

EOF
