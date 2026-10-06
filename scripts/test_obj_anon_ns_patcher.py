"""Rule 7 of scripts/obj_anon_ns_patcher.py votes on EVIDENCE only (lane W16-PM).

The patcher's docstring calls rules 5-7 (`token`, `token_global`, `majority`)
non-evidence fallbacks, yet until 2026-10-06 the `majority` vote counted
`token` edits.  One map edit could then flip a whole object's fallback hash
(W16-PI §6, WaveFile.obj).  These cases are built so the OLD vote gives a
different answer from the new one; each was checked to fail against the
pre-W16-PM patcher (docs/decomp/W16PM_ANON_NS_EVIDENCE_VOTE_2026-10-06.md).
"""

import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import obj_anon_ns_patcher as P  # noqa: E402

EVID = b'aaaaaaaa'   # retail hash reachable by a template (rule 1)
TOKEN = b'bbbbbbbb'  # retail hash reachable only through the token `Tok`
OURS = b'11111111'   # what our compiler wrote


def _blob(names):
    return b'\0' + b'\0'.join(names) + b'\0'


def _index(tmp_path, retail_names):
    path = tmp_path / 'retail.obj'
    path.write_bytes(_blob(retail_names))
    templates, tokens, weights = P.index_object(path)
    g_t, g_k = defaultdict(set), defaultdict(set)
    for k, v in templates.items():
        g_t[k] |= v
    for k, v in tokens.items():
        g_k[k] |= v
    return (templates, tokens, weights), (g_t, g_k)


def _plan(tmp_path, retail_names, our_names):
    orig_index, global_index = _index(tmp_path, retail_names)
    data = _blob(our_names)
    edits, stats, unresolved = P.plan_object(data, orig_index, global_index)
    assert not unresolved
    by_name = {}
    for start, end in P.hash_runs(data):
        run = data[start:end]
        by_name[run] = [edits[start + off] for off, _ in P.hashes_of(run)]
    return by_name, stats


def _n(scope, h, fn=b'Foo'):
    return b'?' + fn + b'@' + scope + b'@?A0x' + h + b'@@QAAXXZ'


def test_token_edits_do_not_outvote_evidence(tmp_path):
    retail = [_n(b'Known', EVID), _n(b'Tok', TOKEN, b'Bar')]
    tok_names = [_n(b'Tok', OURS, b'Baz%d' % i) for i in range(5)]
    ours = [_n(b'Known', OURS)] + tok_names + [_n(b'Unknown', OURS, b'Qux')]
    by_name, stats = _plan(tmp_path, retail, ours)
    # 1 template vote for EVID against 5 token edits to TOKEN: the old vote
    # (Counter(edits.values())) picked TOKEN here.
    assert by_name[_n(b'Unknown', OURS, b'Qux')] == [EVID]
    assert stats['majority'] == 1
    assert stats['token'] == 5


def test_token_edits_still_respell_their_own_symbol(tmp_path):
    retail = [_n(b'Known', EVID), _n(b'Tok', TOKEN, b'Bar')]
    ours = [_n(b'Known', OURS), _n(b'Tok', OURS, b'Baz')]
    by_name, _ = _plan(tmp_path, retail, ours)
    assert by_name[_n(b'Known', OURS)] == [EVID]
    assert by_name[_n(b'Tok', OURS, b'Baz')] == [TOKEN]


def test_no_evidence_falls_back_to_retail_weight_not_tokens(tmp_path):
    # Retail's object is dominated by EVID (3 names) but none of them is a
    # template for anything we emit; TOKEN is reachable by token only.
    retail = [_n(b'A', EVID, b'X'), _n(b'B', EVID, b'Y'), _n(b'C', EVID, b'Z'),
              _n(b'Tok', TOKEN, b'Bar')]
    ours = [_n(b'Tok', OURS, b'Baz%d' % i) for i in range(4)]
    ours.append(_n(b'Unknown', OURS, b'Qux'))
    by_name, stats = _plan(tmp_path, retail, ours)
    # The old vote had 4 token edits and picked TOKEN.
    assert by_name[_n(b'Unknown', OURS, b'Qux')] == [EVID]
    assert stats['majority_retail_weight'] == 1
    assert 'majority' not in stats


def test_vote_ignores_our_current_spelling(tmp_path):
    # Idempotence: the decision must not depend on the hash already in place.
    retail = [_n(b'Known', EVID), _n(b'Tok', TOKEN, b'Bar')]
    first = [_n(b'Known', OURS), _n(b'Tok', OURS, b'Baz'),
             _n(b'Unknown', OURS, b'Qux')]
    second = [n.replace(OURS, TOKEN) for n in first]
    a, _ = _plan(tmp_path, retail, first)
    b, _ = _plan(tmp_path, retail, second)
    assert list(a.values()) == list(b.values())
