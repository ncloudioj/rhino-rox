import unittest
import redis


class TestTrieCmd(unittest.TestCase):
    rr = redis.Redis("localhost", 6000)

    def tearDown(self):
        self.rr.execute_command("del trie")

    def test_basic_cmds(self):
        self.rr.execute_command("rset trie ape 1")
        self.rr.execute_command("rset trie app 2")
        ret = self.rr.execute_command("rget trie app")
        self.assertEqual(ret, "2")

        ret = self.rr.execute_command("rlen trie")
        self.assertEqual(ret, 2)

        ret = self.rr.execute_command("rexists trie ape")
        self.assertTrue(ret)

        ret = self.rr.execute_command("rdel trie ape")
        ret = self.rr.execute_command("rexists trie ape")
        self.assertFalse(ret)

    def test_prefix(self):
        self.rr.execute_command("rset trie apply 1")
        self.rr.execute_command("rset trie apple 2")
        self.rr.execute_command("rset trie ape 3")
        self.rr.execute_command("rset trie apolo 4")
        self.rr.execute_command("rset trie arm 5")

        ret = self.rr.execute_command("rpget trie ap")
        self.assertListEqual(ret, ["ape", "3", "apolo", "4", "apple", "2",
                                   "apply", "1"])

    def test_iterator(self):
        self.rr.execute_command("rset trie apply 1")
        self.rr.execute_command("rset trie apple 2")
        self.rr.execute_command("rset trie ape 3")

        ret = self.rr.execute_command("rkeys trie")
        self.assertListEqual(ret, ["ape", "apple", "apply"])

        ret = self.rr.execute_command("rvalues trie")
        self.assertListEqual(ret, ["3", "2", "1"])

        ret = self.rr.execute_command("rgetall trie")
        self.assertListEqual(ret, ["ape", "3", "apple", "2", "apply", "1"])
