"""Create a missing lightweight release tag or verify its peeled commit."""
import json
import os
import re
import sys
from urllib.error import HTTPError
from urllib.parse import quote
from urllib.request import Request, urlopen


class ApiError(RuntimeError):
    def __init__(self, status):
        self.status = status
        super().__init__(f'GitHub API request failed (HTTP {status})')


def api_request(method, path, payload=None):
    data = None if payload is None else json.dumps(payload).encode('utf-8')
    request = Request('https://api.github.com/' + path, data=data, method=method,
                      headers={'Authorization': 'Bearer ' + os.environ['GH_TOKEN'],
                               'Accept': 'application/vnd.github+json',
                               'Content-Type': 'application/json',
                               'X-GitHub-Api-Version': '2022-11-28'})
    try:
        with urlopen(request, timeout=30) as response:
            return json.load(response)
    except HTTPError as error:
        raise ApiError(error.code) from None


def ensure_release_tag(repository, tag, commit, request=api_request):
    if not re.fullmatch(r'[\w.-]+/[\w.-]+', repository):
        raise ValueError('Invalid repository name')
    if not re.fullmatch(r'v\d+\.\d+\.\d+', tag):
        raise ValueError('Invalid release version tag')
    if not re.fullmatch(r'[0-9a-f]{40}', commit):
        raise ValueError('Invalid release commit SHA')
    base = f'repos/{repository}/git/'
    try:
        obj = request('GET', base + 'ref/tags/' + quote(tag, safe=''))['object']
    except ApiError as error:
        if error.status != 404:
            raise
        request('POST', base + 'refs', {'ref': 'refs/tags/' + tag, 'sha': commit})
        return
    seen = set()
    while obj['type'] == 'tag':
        sha = obj['sha']
        if sha in seen or not re.fullmatch(r'[0-9a-f]{40}', sha):
            raise ValueError('Invalid or cyclic annotated tag')
        seen.add(sha)
        obj = request('GET', base + 'tags/' + sha)['object']
    if obj['type'] != 'commit' or obj['sha'] != commit:
        raise ValueError('Release tag does not point to the requested commit')


if __name__ == '__main__':
    ensure_release_tag(os.environ['GITHUB_REPOSITORY'], os.environ['RELEASE_TAG'],
                       os.environ['GITHUB_SHA'])
