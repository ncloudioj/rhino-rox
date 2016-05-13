import unittest
import redis


class TestCmdDB(unittest.TestCase):
    rr = redis.Redis("localhost", 6000)

    def tearDown(self):
        self.rr.execute_command("del foo")
        self.rr.execute_command("del egg")
        self.rr.execute_command("del apple")

    def test_basic_cmds(self):
        self.rr.set("foo", "bar")
        self.rr.set("egg", "spam")
        self.rr.set("apple", "orange")
        ret = self.rr.execute_command("len")
        self.assertEqual(ret, 3)

        ret = self.rr.get("foo")
        ret = self.rr.get("foo")
        ret = self.rr.get("foo")
        self.assertEqual(ret, "bar")

        ret = self.rr.type("foo")
        self.assertEqual(ret, "string")

        ret = self.rr.exists("foo")
        self.assertTrue(ret)

        self.rr.delete("foo")
        ret = self.rr.get("foo")
        self.assertIsNone(ret)

        ret = self.rr.exists("foo")
        self.assertFalse(ret)
