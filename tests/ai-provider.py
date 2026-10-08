"""Offline provider and retry regressions; no Gemini API calls."""
import ast, io, json, os, sys, tempfile, unittest, urllib.error, urllib.request
from pathlib import Path
from unittest.mock import patch, Mock
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'tools'))
import ai_http
source=Path(__file__).resolve().parents[1]/'tools/ai-agent.py'
function=next(n for n in ast.parse(source.read_text(encoding='utf-8')).body if isinstance(n,ast.FunctionDef) and n.name=='ollama')
namespace=dict(request={'provider':'ollama','model':'test-model'},base='http://localhost:11434',os=os,json=json,urllib=__import__('urllib'),gemini_request=ai_http.gemini_request,emit=lambda *args,**kw:None)
exec(compile(ast.Module(body=[function],type_ignores=[]),str(source),'exec'),namespace)
class Clock:
    def __init__(self): self.now=1000000.;self.waits=[]
    def time(self):return self.now
    def sleep(self,seconds):self.waits.append(seconds);self.now+=seconds
class Tests(unittest.TestCase):
    def setUp(self):self.temp=tempfile.TemporaryDirectory();self.clock=Clock()
    def tearDown(self):self.temp.cleanup()
    def quota(self,limits=(30,1000,8000,200000)):
        return ai_http.Quota(self.temp.name,'key','model',limits,clock=self.clock.time,sleep=self.clock.sleep)
    def test_pause_and_persistence(self):
        self.quota().reserve(100)
        self.quota().reserve(100)
        self.assertGreaterEqual(self.clock.now,1000002.1)
        self.assertEqual(len(json.loads(self.quota().path.read_text())['entries']),2)
    def test_daily_stop_and_expiry(self):
        quota=self.quota((30,2,8000,200000));quota.reserve(100);quota.reserve(100)
        with self.assertRaisesRegex(RuntimeError,'суточный'):quota.reserve(100)
        self.clock.now+=86401;quota.reserve(100)
    def test_token_wait(self):
        quota=self.quota((30,1000,500,200000));quota.reserve(300);quota.reserve(300)
        self.assertGreaterEqual(self.clock.now,1000060.1)
        with self.assertRaisesRegex(RuntimeError,'Контекст'):quota.reserve(501)
    def test_server_headers(self):
        quota=self.quota();ticket=quota.reserve(1000)
        quota.update(ticket,{'x-ratelimit-remaining-tokens':'0','x-ratelimit-reset-tokens':'7.66s'},actual=120)
        quota.reserve(100)
        self.assertGreaterEqual(self.clock.now,1000007.66)
        self.assertEqual(json.loads(quota.path.read_text())['entries'][0]['tokens'],120)
    def test_duration(self):self.assertAlmostEqual(ai_http.duration('1m2.5s'),62.5)
    def test_ollama(self):
        def respond(req,**kwargs):
            self.assertTrue(req.full_url.endswith('/api/chat'));self.assertIn('options',json.loads(req.data));self.assertIsNone(req.get_header('Authorization'))
            return io.BytesIO(b'{"message":{"content":"local"}}')
        namespace['request']['provider']='ollama'
        with patch('urllib.request.urlopen',respond):self.assertEqual(namespace['ollama']([])['content'],'local')
    def test_gemini_retry_and_payload(self):
        response=io.BytesIO(b'{"choices":[{"message":{"content":"ok"}}]}');response.headers={}
        error=urllib.error.HTTPError('https://generativelanguage.googleapis.com',429,'limit',{'retry-after':'0'},None)
        opener=Mock();opener.open.side_effect=[error,response]
        with patch('ai_http.build_opener',return_value=opener),patch('ai_http.time.sleep'):
            result=ai_http.gemini_request({'model':'m','messages':[],'max_completion_tokens':1024},'test-key')
        self.assertEqual(result['content'],'ok');self.assertEqual(opener.open.call_count,2)
        req=opener.open.call_args[0][0]
        self.assertEqual(req.get_header('Authorization'),'Bearer test-key')
        self.assertEqual(req.get_header('User-agent'),'EduCode/0.3 (Gemini client)')
        self.assertIn('generativelanguage.googleapis.com',req.full_url)
        self.assertNotIn('options',json.loads(req.data))
    def test_retry_bounded(self):
        opener=Mock();opener.open.side_effect=urllib.error.HTTPError('x',429,'limit',{'retry-after':'2'},None)
        with patch('ai_http.build_opener',return_value=opener),patch('ai_http.time.sleep'):
            with self.assertRaisesRegex(RuntimeError,'Повторить'):ai_http.gemini_request({'model':'m','messages':[]},'key')
        self.assertEqual(opener.open.call_count,4)
    def test_gemini_retries_503(self):
        response=io.BytesIO(b'{"choices":[{"message":{"content":"recovered"}}]}');response.headers={}
        opener=Mock();opener.open.side_effect=[urllib.error.HTTPError('x',503,'busy',{},io.BytesIO(b'{"error":{"message":"overloaded"}}')),response]
        updates=[]
        with patch('ai_http.build_opener',return_value=opener),patch('ai_http.time.sleep'):
            result=ai_http.gemini_request({'model':'gemini-3.1-flash-lite','messages':[]},'key',updates.append)
        self.assertEqual(result['content'],'recovered');self.assertIn('503',updates[0])
    def test_gemini_explains_404_without_retry(self):
        opener=Mock();opener.open.side_effect=urllib.error.HTTPError('x',404,'missing',{},io.BytesIO(b'{}'))
        with patch('ai_http.build_opener',return_value=opener):
            with self.assertRaisesRegex(RuntimeError,'не нашёл модель'):ai_http.gemini_request({'model':'openai/wrong','messages':[]},'key')
        self.assertEqual(opener.open.call_count,1)
    def test_context_keeps_tool_pairs(self):
        body={'model':'m','messages':[{'role':'system','content':'s'},{'role':'user','content':'old'*10000},{'role':'assistant','tool_calls':[{'id':'1'}],'content':''},{'role':'tool','tool_call_id':'1','content':'result'},{'role':'user','content':'latest'}]}
        fitted,cost=ai_http.fit_context(body,8000,1024)
        self.assertEqual([m['role'] for m in fitted['messages']],['system','user']);self.assertLess(cost,8000)
if __name__=='__main__':unittest.main()
