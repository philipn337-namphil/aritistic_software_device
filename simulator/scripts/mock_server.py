"""Local stand-in for the device API (POST /device-api/v1/claim and /sync).

Use it to exercise the virtual device's full claim/sync path without the
real server:  python simulator/scripts/mock_server.py [port]
then in the device console:  api reset-uuid, api endpoint http://127.0.0.1:9090,
api pair 1234567890.  Every pairing code of 10 digits is accepted.

The schedule is generated at the first sync: one big routine for today that
starts one minute later and lasts an hour, with three small routines.
Completions the device posts are stored and echoed back as DONE.
"""
import json
import sys
import uuid
from datetime import datetime, timedelta, timezone
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

KST = timezone(timedelta(hours=9))
STEPS = [(9001, "물 마시기"), (9002, "가방 챙기기"), (9003, "신발 신기")]
state = {"uuid": None, "start": None, "done": {}}


def iso(moment):
    return moment.astimezone(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")


def schedule(now):
    if state["start"] is None:
        start = (now + timedelta(minutes=1)).astimezone(KST).replace(second=0, microsecond=0)
        state["start"] = start
    start = state["start"]
    end = min(start + timedelta(hours=1), start.replace(hour=23, minute=59))
    small = []
    for order, (step, title) in enumerate(STEPS, 1):
        done = state["done"].get(step)
        small.append({"smallRoutineId": step, "title": title, "sortOrder": order,
                      "status": "DONE" if done else "PENDING", "completedAt": done})
    return [{"date": start.strftime("%Y-%m-%d"), "bigRoutines": [{
        "bigRoutineId": 7001, "seriesId": "mock", "title": "외출 준비",
        "startTime": start.strftime("%H:%M"), "endTime": end.strftime("%H:%M"),
        "sortOrder": 1, "smallRoutines": small}]}]


class Handler(BaseHTTPRequestHandler):
    def reply(self, status, data=None, error=None):
        body = json.dumps({"success": error is None, "data": data, "error": error}, ensure_ascii=False).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self):
        length = int(self.headers.get("Content-Length", 0))
        try:
            request = json.loads(self.rfile.read(length) or b"{}")
        except ValueError:
            return self.reply(400, error={"code": "BAD_JSON", "message": "bad json", "details": None})
        print(self.path, json.dumps(request, ensure_ascii=False), flush=True)
        now = datetime.now(timezone.utc)
        if self.path == "/device-api/v1/claim":
            code = str(request.get("pairingCode", ""))
            if len(code) != 10 or not code.isdigit():
                return self.reply(404, error={"code": "PAIRING_CODE_NOT_FOUND", "message": "invalid", "details": None})
            state["uuid"] = str(uuid.uuid4())
            return self.reply(200, {"deviceId": 1, "deviceAccessUuid": state["uuid"]})
        if self.path == "/device-api/v1/sync":
            if not state["uuid"] or self.headers.get("X-Device-Uuid") != state["uuid"]:
                return self.reply(401, error={"code": "UNAUTHORIZED", "message": "unknown device", "details": None})
            accepted = 0
            for completion in request.get("completions", []):
                step = completion.get("smallRoutineId")
                if step not in state["done"]:
                    state["done"][step] = completion.get("completedAt")
                    accepted += 1
            return self.reply(200, {"serverTime": iso(now), "accepted": accepted, "routines": schedule(now)})
        return self.reply(404, error={"code": "NOT_FOUND", "message": self.path, "details": None})


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 9090
    print(f"mock device API on http://127.0.0.1:{port}", flush=True)
    ThreadingHTTPServer(("127.0.0.1", port), Handler).serve_forever()
