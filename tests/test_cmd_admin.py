import unittest
import redis


class TestAdminCmd(unittest.TestCase):
    rr = redis.Redis("localhost", 6000)

    def test_ping(self):
        ret = self.rr.ping()
        self.assertTrue(ret)

    def test_echo(self):
        ret = self.rr.echo("echo")
        self.assertEqual(ret, "echo")

    def test_info(self):
        info = self.rr.info()
        self.assertIsNotNone(info)
