import unittest
import redis


class TestCmdDB(unittest.TestCase):
    def setUp(self):
        self.client = redis.Redis("localhost", 6000)

    def test_basic_cmds(self):
        self.client.set("foo", "bar")
        self.client.set("egg", "spam")
        self.client.set("apple", "orange")
        ret = self.client.execute_command("len")
        self.assertEqual(ret, 3)

        ret = self.client.get("foo")
        ret = self.client.get("foo")
        ret = self.client.get("foo")
        self.assertEqual(ret, "bar")

        ret = self.client.type("foo")
        self.assertEqual(ret, "string")

        ret = self.client.exists("foo")
        self.assertTrue(ret)

        self.client.delete("foo")
        ret = self.client.get("foo")
        self.assertIsNone(ret)

        ret = self.client.exists("foo")
        self.assertFalse(ret)
