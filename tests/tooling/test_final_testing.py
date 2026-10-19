"""@brief Проверяет достоверность итогового отчёта без запуска приложений или Qt."""
import contextlib
import importlib.util
import io
import json
from pathlib import Path
import subprocess
import tempfile
import unittest
from unittest import mock

SPEC = importlib.util.spec_from_file_location('final_testing', Path(__file__).resolve().parents[2] / 'scripts/final-testing.py')
REPORTER = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(REPORTER)


class FinalTestingTest(unittest.TestCase):
    """@brief Проверяет полноту матрицы, свежесть XML и правильный учёт результатов."""

    def test_junit_statuses_and_service_cases(self):
        """@brief Различает ошибку, падение, пропуск, успех и служебную инициализацию."""
        with tempfile.TemporaryDirectory() as directory:
            path = Path(directory) / 'tests.xml'
            path.write_text('''<testsuite name="Example"><testcase name="initTestCase"/>
                <testcase name="ok" time="0.1"/><testcase name="failed"><failure/></testcase>
                <testcase name="broken"><error/></testcase><testcase name="missing"><skipped/></testcase></testsuite>''')
            cases = REPORTER.read_cases(path)
            self.assertEqual([case['status'] for case in cases], ['PASS', 'PASS', 'FAIL', 'ERROR', 'SKIP'])
            self.assertEqual([case['service'] for case in cases], [True, False, False, False, False])

    def test_stale_xml_and_timeouts_cannot_pass(self):
        """@brief Не принимает старые XML за новый прогон и сохраняет все конфигурации после сбоя."""
        for timed_out in (False, True):
            with self.subTest(timeout=timed_out), tempfile.TemporaryDirectory() as directory:
                project = Path(directory)
                for build in ('Desktop_Qt_6_10_1_MinGW_64_bit-Debug', 'release'):
                    tests = project / 'build' / build / 'tests'
                    tests.mkdir(parents=True)
                    for name in ('images', 'gamecontroller', 'puzzle', 'appcontroller', 'mainwindow'):
                        (tests / (name + '.xml')).write_text('<testsuite name="Old"><testcase name="old"/></testsuite>')
                output = project / 'results'
                output.mkdir()
                outcome = subprocess.CompletedProcess([], 0, b'', b'')
                error = subprocess.TimeoutExpired([], 180) if timed_out else None
                with mock.patch.object(REPORTER.subprocess, 'run', return_value=outcome, side_effect=error), contextlib.redirect_stdout(io.StringIO()):
                    runs = REPORTER.run_matrix(project, output, False)
                self.assertEqual(len(runs), 11)
                self.assertTrue(all(not run['complete'] and not run['cases'] for run in runs))
                self.assertTrue(all(run['exitCode'] == (124 if timed_out else 0) for run in runs))

    def test_summary_deduplicates_repeated_cases_and_reports_run_errors(self):
        """@brief Считает предметные строки отдельно от повторов и помечает неполный запуск ошибкой."""
        with tempfile.TemporaryDirectory() as directory, contextlib.redirect_stdout(io.StringIO()):
            project = Path(directory)
            (project / 'tests').mkdir()
            output = project / 'output'
            output.mkdir()
            cases = [dict(suite='Example', name=name, status='PASS', seconds=0.1, service=service)
                     for name, service in [('initTestCase', True), ('sample(row)', False), ('cleanupTestCase', True)]]
            runs = [dict(run=f'run-{i}', seconds=0.3, exitCode=0, complete=True, cases=cases) for i in range(11)]
            checks = [dict(build=name, passed=True, report={}) for name in ('Debug', 'Release')]
            REPORTER.write_reports(project, output, runs, checks, 'hash', False)
            result = json.loads((output / 'results.json').read_text(encoding='utf-8'))
            self.assertEqual(result['status'], 'PASS')
            self.assertEqual(result['uniqueTests'], 1)
            self.assertEqual(result['serviceChecks'], 2)
            self.assertEqual(result['executions'], 11)
            self.assertTrue(result['date'].endswith('+03:00'))
            runs[0]['complete'] = False
            REPORTER.write_reports(project, output, runs, checks, 'hash', False)
            result = json.loads((output / 'results.json').read_text(encoding='utf-8'))
            self.assertEqual(result['status'], 'FAIL')
            self.assertEqual(result['runErrors'], ['run-0'])

    def test_manifest_changes_when_tested_source_changes(self):
        """@brief Манифест воспроизводится и меняется после правки исходника рабочей копии."""
        with tempfile.TemporaryDirectory() as directory:
            project = Path(directory)
            for name in ('CMakeLists.txt', 'README.md', 'AGENTS.md'):
                (project / name).write_text(name)
            (project / 'src').mkdir()
            source = project / 'src' / 'example.cpp'
            source.write_text('first')
            output = project / 'output'
            output.mkdir()
            first = REPORTER.source_manifest(project, output)
            self.assertEqual(first, REPORTER.source_manifest(project, output))
            source.write_text('second')
            self.assertNotEqual(first, REPORTER.source_manifest(project, output))


if __name__ == '__main__':
    unittest.main()
