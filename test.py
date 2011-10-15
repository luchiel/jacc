#!/usr/bin/env python
import glob
import os
import difflib

tests = {
	'lexer': 'jacc lex "%(input)s" > "%(output)s" 2>&1',
    'expr': 'jacc parse_expr "%(input)s" > "%(output)s" 2>&1',
}

tester_dir = os.path.dirname(os.path.abspath(__file__))
tests_dir = os.path.join(tester_dir, 'tests')

def change_ext(path, new_ext):
    root, ext = os.path.splitext(path)
    return root + new_ext

def unlink(path):
    if os.path.exists(path):
        os.unlink(path)

def run_tests(dir, cmd_template):
    path = os.path.join(tests_dir, dir)
    total = 0
    failed = []

    for test in glob.iglob(os.path.join(path, '*.in')):
        test_name = os.path.basename(test)
        total += 1

        output_file = change_ext(test, '.out')
        answer_file = change_ext(test, '.answer')
        diff_file = change_ext(test, '.diff')

        cmd = cmd_template % {
            'input': test,
            'output': output_file,
        }

        unlink(output_file)
        unlink(diff_file)

        ret_code = os.system(cmd)
        if ret_code != 0:
            failed.append('%s (%d returned)' % (test_name, ret_code))
            continue

        try:
            output = map(str.strip, open(output_file, 'r').readlines())
            answer = map(str.strip, open(answer_file, 'r').readlines())
        except IOError:
            failed.append('%s (IO)' % test_name)
            continue

        if output != answer:
            diff = difflib.unified_diff(output, answer, lineterm='')
            with open(diff_file, 'w') as diff_file:
                diff_file.write('\n'.join(diff))
                diff_file.write('\n')

            failed.append(test_name)
            continue
        else:
            unlink(output_file)

    return {
        'total': total,
        'failed': sorted(failed),
    }

def get_status(failed):
    return "FAIL" if failed else "OK"

if __name__ == '__main__':
    total = 0
    total_failed = 0
    for dir, cmd in tests.iteritems():
        stats = run_tests(dir, os.path.join(tester_dir, cmd))

        total += stats['total']
        total_failed += len(stats['failed'])

        succeed = stats['total'] - len(stats['failed'])
        status = get_status(len(stats['failed']))

        fail_list = ""
        if stats['failed']:
            fail_list = " - %s" % (', '.join(stats['failed']))
        print "%s: %s (%d/%d)%s" % (dir, status, succeed, stats['total'], fail_list)

    succeed = total - total_failed
    print "---"
    print "%s (%d/%d)" % (get_status(total_failed), succeed, total)