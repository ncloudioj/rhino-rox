import unittest
import redis


class TestAdminCmd(unittest.TestCase):
    def setUp(self):
        self.client = redis.Redis("localhost", 6000)

    def tearDown(self):
        pass

    def test_ping(self):
        ret = self.client.ping()
        self.assertTrue(ret)

    def test_echo(self):
        ret = self.client.echo("echo")
        self.assertEqual(ret, "echo")

    def test_info(self):
        info = self.client.info()
        self.assertIsNotNone(info)
