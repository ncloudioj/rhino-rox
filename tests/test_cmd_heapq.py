import unittest
import redis
import random


class TestHeadpCmd(unittest.TestCase):
    rr = redis.Redis("localhost", 6000)

    def tearDown(self):
        self.rr.execute_command("del test")

    def test_basic_cmds(self):
        self.rr.execute_command("qpush test 1 foo")
        ret = self.rr.execute_command("qpeek test")
        length = self.rr.execute_command("qlen test")
        self.assertEqual(ret, "foo")
        self.assertEqual(length, 1)

        ret = self.rr.execute_command("qpop test")
        length = self.rr.execute_command("qlen test")
        self.assertEqual(ret, "foo")
        self.assertEqual(length, 0)
        ret = self.rr.execute_command("qpeek test")
        self.assertIsNone(ret)

    def _load_heap(self):
        self.rr.execute_command("qpush test 1 v1")
        self.rr.execute_command("qpush test 4 v2")
        self.rr.execute_command("qpush test 2 v3")
        self.rr.execute_command("qpush test 1.5 v4")

    def test_advanced_cmds(self):
        self._load_heap()
        ret = self.rr.execute_command("qpopn test 4")
        self.assertListEqual(ret, ["v1", "v4", "v3", "v2"])

        self._load_heap()
        ret = self.rr.execute_command("qpopn test 100")
        self.assertListEqual(ret, ["v1", "v4", "v3", "v2"])

        self._load_heap()
        ret = self.rr.execute_command("qpopn test 2")
        self.assertListEqual(ret, ["v1", "v4"])
        length = self.rr.execute_command("qlen test")
        self.assertEqual(length, 2)
        ret = self.rr.execute_command("qpeek test")
        self.assertEqual(ret, "v3")
        ret = self.rr.execute_command("qpop test")
        self.assertEqual(ret, "v3")
        ret = self.rr.execute_command("qpop test")
        self.assertEqual(ret, "v2")
        length = self.rr.execute_command("qlen test")
        self.assertEqual(length, 0)

    def test_pressure_test(self):
        _N = 10000
        input = range(_N)
        random.shuffle(input)
        for item in input:
            self.rr.execute_command("qpush test %d %d" % (item, item))
        for i in range(_N):
            ret = self.rr.execute_command("qpop test")
            self.assertEqual(ret, "%d" % i)

        for item in input:
            self.rr.execute_command("qpush test %d %d" % (item, item))
        ret = self.rr.execute_command("qpopn test %d" % len(input))
        self.assertListEqual(ret, ["%d" % i for i in range(_N)])
