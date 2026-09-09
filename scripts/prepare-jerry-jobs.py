#!/usr/bin/env python3
"""Stage pinned JerryScript with one bounded-job API; never edit the SDK cache.

Upstream jerry_run_jobs() remains drain-to-empty. MCU.js uses the separate
mcujs_jerry_run_jobs(max_jobs, pending) extension, yielding between whole jobs.
"""
import argparse
import hashlib
import json
from pathlib import Path
import shutil

PINNED = {
    'jerry-core/api/jerryscript.c': '8f1690961c49a855934681db29f0a28df4a01932cdb2b6ae798438907a0bc9c9',
    'jerry-core/include/jerryscript-core.h': 'fba35f7a78bf80393d0fc84108c16f7103376c794796522b3855cd8eebadcc70',
    'jerry-core/ecma/operations/ecma-jobqueue.c': 'ddb3b0f15b149e1f037758535935c13218461cbe6cbbc24dda790c0581800ec5',
    'jerry-core/ecma/operations/ecma-jobqueue.h': 'cb845d844b51b53ae84707830e2da23d6aa1dc247311066a1413d7a9ec7c2b3e',
}

def replace(text, old, new):
    if text.count(old) != 1:
        raise ValueError('Pinned JerryScript patch anchor differs: ' + old[:80])
    return text.replace(old, new, 1)

def prepare(source, destination):
    source, destination = source.resolve(), destination.resolve()
    if destination == source or source in destination.parents:
        raise ValueError('The staging destination must be outside the SDK source')
    patched = {}
    for name, digest in PINNED.items():
        data = (source / name).read_bytes()
        if hashlib.sha256(data).hexdigest() != digest:
            raise ValueError('Unsupported or modified JerryScript source: ' + name)
        patched[name] = data.decode()
    api = 'jerry-core/api/jerryscript.c'
    patched[api] = replace(patched[api], '} /* jerry_run_jobs */', '''} /* jerry_run_jobs */

/* MCU.js extension: zero budget only queries pending state. Not upstream API. */
jerry_value_t
mcujs_jerry_run_jobs (uint32_t max_jobs, bool *pending_p)
{
  jerry_assert_api_enabled ();
  ecma_value_t result = max_jobs ? ecma_process_some_enqueued_jobs (max_jobs) : ECMA_VALUE_UNDEFINED;
  *pending_p = JERRY_CONTEXT (job_queue_head_p) != NULL;
  return jerry_return (result);
}''')
    header = 'jerry-core/include/jerryscript-core.h'
    patched[header] = replace(patched[header], 'jerry_value_t jerry_run_jobs (void);',
        'jerry_value_t jerry_run_jobs (void);\n/* MCU.js-only bounded-job extension; pending_p must not be NULL. */\n'
        'jerry_value_t mcujs_jerry_run_jobs (uint32_t max_jobs, bool *pending_p);')
    queue = 'jerry-core/ecma/operations/ecma-jobqueue.c'
    patched[queue] = replace(patched[queue], '''ecma_process_all_enqueued_jobs (void)
{
  ecma_value_t ret = ECMA_VALUE_UNDEFINED;

  while (JERRY_CONTEXT (job_queue_head_p) != NULL)''', '''ecma_process_all_enqueued_jobs (void)
{
  return ecma_process_some_enqueued_jobs (0);
}

/* A zero internal limit preserves upstream's drain-to-empty behavior. */
ecma_value_t
ecma_process_some_enqueued_jobs (uint32_t limit)
{
  ecma_value_t ret = ECMA_VALUE_UNDEFINED;
  uint32_t processed = 0;

  while (JERRY_CONTEXT (job_queue_head_p) != NULL && (limit == 0 || processed++ < limit))''')
    header = 'jerry-core/ecma/operations/ecma-jobqueue.h'
    patched[header] = replace(patched[header], 'ecma_value_t ecma_process_all_enqueued_jobs (void);',
        'ecma_value_t ecma_process_all_enqueued_jobs (void);\n'
        'ecma_value_t ecma_process_some_enqueued_jobs (uint32_t limit);')
    receipt = {name: hashlib.sha256(text.encode()).hexdigest() for name, text in patched.items()}
    marker = destination / '.mcujs-jobs-patch.json'
    if destination.exists():
        if not marker.exists() or json.loads(marker.read_text()) != receipt:
            raise ValueError('Staging directory differs; use a clean build directory')
        for name, digest in receipt.items():
            if hashlib.sha256((destination / name).read_bytes()).hexdigest() != digest:
                raise ValueError('Staged JerryScript changed: ' + name)
        return
    shutil.copytree(source, destination, ignore=shutil.ignore_patterns('.git', 'build', '__pycache__'))
    for name, text in patched.items():
        (destination / name).write_text(text)
    marker.write_text(json.dumps(receipt, indent=2) + '\n')

if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('source', type=Path)
    parser.add_argument('destination', type=Path)
    args = parser.parse_args()
    prepare(args.source, args.destination)
