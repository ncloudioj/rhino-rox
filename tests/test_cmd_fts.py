import unittest
import redis


_Docs = [
    ("enemy", """If you know the enemy and know yourself you need not fear the
              results of a hundred battles"""),
    ("fighting", """The supreme art of war is to subdue the enemy without
                 fighting"""),
    ("attack", """Invincibility lies in the defence; the possibility of victory
               in the attack"""),
    ("self", """Know thy self, know thy enemy. A thousand battles, a thousand
             victories"""),
    ("hand", """The opportunity to secure ourselves against defeat lies in our
             own hands, but the opportunity of defeating the enemy is provided
             by the enemy himself"""),
    ("excellence", """To fight and conquer in all our battles is not supreme
                  excellence; supreme excellence consists in breaking the enemy's
                  resistance without fighting"""),
    ("warriors", """Victorious warriors win first and then go to war, while
                 defeated warriors go to war first and then seek to win"""),
    ("oppenents", """Be extremely subtle, even to the point of formlessness. Be
                  extremely mysterious, even to the point of soundlessness.
                  Thereby you can be the director of the opponent's fate"""),
    ("patience", """He who is prudent and lies in wait for an enemy who is not,
                 will be victorious"""),
    ("pretend", """Pretend inferiority and encourage his arrogance""")
]


class TestFTSCmd(unittest.TestCase):
    rr = redis.Redis("localhost", 6000)

    def tearDown(self):
        pass
        # self.rr.execute_command("del", "fts")

    def test_basic_cmds(self):
        for title, quote in _Docs:
            self.rr.execute_command("dset", "fts", title, quote)

        ret = self.rr.execute_command("dlen", "fts")
        self.assertEquals(ret, len(_Docs))

        ret = self.rr.execute_command("dget", "fts", "pretend")
        self.assertEquals(ret, _Docs[-1][1])

        ret = self.rr.execute_command("dsearch", "fts", "battle")
        self.assertEquals(len(ret), 6)  # 3 * (title, doc)

        ret = self.rr.execute_command("dsearch", "fts", "enemy")
        self.assertEquals(len(ret), 10)  # 5 * (title, doc)
        ret = self.rr.execute_command("dsearch", "fts", "opportunity")
        self.assertEquals(len(ret), 2)

        self.rr.execute_command("ddel", "fts", "pretend")
        ret = self.rr.execute_command("dlen", "fts")
        self.assertEquals(ret, len(_Docs) - 1)
        ret = self.rr.execute_command("dsearch", "fts", "inferiority")
        self.assertEquals(len(ret), 0)
