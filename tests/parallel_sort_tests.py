import io
import os
import random
import string
import sys
import tempfile
import subprocess as sp
import unittest
import platform

# The parallel sort code requires GNU sort + GNU split implementations
# which are located in gsplit and gsort on OSX from brew.
CMD_NAME_SPLIT = 'gsplit' if platform.system() == 'Darwin' else 'split'
CMD_NAME_SORT = 'gsort' if platform.system() == 'Darwin' else 'split'

BINARY_UNDER_TEST = 'binaries/parallel_sort.py'

class ParallelSortTests(unittest.TestCase):
    def setUp(self):
        # Make some temporary files before each test
        self.test_ifile = tempfile.NamedTemporaryFile(mode = 'w')
        self.test_ifile_name = self.test_ifile.name
        self.test_ifile_two = tempfile.NamedTemporaryFile(mode = 'w')
        self.test_ifile_two_name = self.test_ifile_two.name
        self.test_ofile = tempfile.NamedTemporaryFile(mode = 'r')
        self.test_ofile_name = self.test_ofile.name

    def tearDown(self):
        # Make sure tempfiles will delete themselves on close after ever test
        self.test_ifile.close()
        self.test_ofile.close()
        self.test_ifile_two.close()

        # Make sure the files are deleted
        self.assertFalse(os.path.exists(self.test_ifile_name))
        self.assertFalse(os.path.exists(self.test_ifile_two_name))
        self.assertFalse(os.path.exists(self.test_ofile_name))

    def check_file_sorted(self, fname, expected, key = lambda x: x):
        # Check if the file is sorted
        prev = ''
        num_lines = 0
        expected = sorted(expected, key = key, reverse = False)
        with open(fname, 'r') as ofile:
            for line in ofile:
                word = line.strip('\n')
                if num_lines != 0:
                    self.assertGreaterEqual(key(word), key(prev))
                prev = word
                self.assertEqual(expected[num_lines], word)
                num_lines += 1
        return num_lines

    def generate_random_data(
            self, size, alpha = string.ascii_lowercase, length = 10):
        return [''.join([random.choice(alpha) for _ in range(length)])
                for _ in range(size)]

    def write_to_file(self, fileobj, data):
        for d in data:
            fileobj.write('%s\n' % d)
        # Make sure we finishing writing the data to the file
        fileobj.flush()

    def test_sort_file(self):
        # Generate some random data and write it to the file
        random_data = self.generate_random_data(10)
        self.write_to_file(self.test_ifile, random_data)

        # Sort the file
        ret = sp.call(
            [BINARY_UNDER_TEST, '--ofile', self.test_ofile_name,
             self.test_ifile_name,
             '--split-exec', CMD_NAME_SPLIT,
             '--sort-executable', CMD_NAME_SORT])

        # Make sure we exited properly
        self.assertEqual(ret, 0)
        lines = self.check_file_sorted(self.test_ofile_name, random_data)
        self.assertEqual(lines, len(random_data))

    def test_merge_files(self):
        # Generate some random data
        chunk_one = sorted(self.generate_random_data(10))
        chunk_two = sorted(self.generate_random_data(10))
        self.write_to_file(self.test_ifile, chunk_one)
        self.write_to_file(self.test_ifile_two, chunk_two)
        # Make sure the chunks sorted before we merge them
        self.check_file_sorted(self.test_ifile_name, chunk_one)
        self.check_file_sorted(self.test_ifile_two_name, chunk_two)

        # Merge the two files
        ret = sp.call(
            [BINARY_UNDER_TEST, '--ofile', self.test_ofile_name,
             self.test_ifile_name, self.test_ifile_two_name, '--merge',
             '--split-exec', CMD_NAME_SPLIT,
             '--sort-executable', CMD_NAME_SORT])

        # Make sure we exited properly
        self.assertEqual(ret, 0)
        lines = self.check_file_sorted(
            self.test_ofile_name, chunk_one + chunk_two)
        self.assertEqual(len(chunk_one) + len(chunk_two), lines)

    def test_sort_numeric(self):
        # Generate some random data and write it to the file
        chunk_one = self.generate_random_data(10, string.digits, 10)
        self.write_to_file(self.test_ifile, chunk_one)
        # Make sure that the number sort result would be different than lexical
        chunk_two = self.generate_random_data(10, string.digits, 8)
        self.write_to_file(self.test_ifile, chunk_two)

        # Sort the file
        ret = sp.call(
            [BINARY_UNDER_TEST, '--ofile', self.test_ofile_name,
             self.test_ifile_name, '--sort-flags', 'gs',
             '--split-exec', CMD_NAME_SPLIT,
             '--sort-executable', CMD_NAME_SORT])

        # Make sure we exited properly
        self.assertEqual(ret, 0)
        lines = self.check_file_sorted(
            self.test_ofile_name, chunk_one + chunk_two, int)
        self.assertEqual(lines, len(chunk_one) + len(chunk_two))

if __name__ == '__main__':
    unittest.main()
