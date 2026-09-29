// k6 smoke/load test — run via `task test:load` (see Taskfile.yml). Only
// hits /livez today: a full auth-then-CreateTask scenario needs a
// ConnectRPC-over-JSON client (k6 has no protobuf codegen story), which is
// worth adding once there's a second real scenario to justify the setup
// cost — see docs/events.md for the outbox-backed events this could
// eventually exercise end-to-end (publish → forwarder → inbox dedup).
import http from "k6/http";
import { check, sleep } from "k6";

export const options = {
  vus: 10,
  duration: "30s",
  thresholds: {
    http_req_failed: ["rate<0.01"],
    http_req_duration: ["p(95)<200"],
  },
};

const BASE_URL = __ENV.BASE_URL || "http://localhost:8080";

export default function () {
  const res = http.get(`${BASE_URL}/livez`);
  check(res, { "status is 200": (r) => r.status === 200 });
  sleep(1);
}
