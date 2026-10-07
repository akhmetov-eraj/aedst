// In-memory stand-in for apps/server's ConnectRPC services, shared by the
// Playwright mock server (./server.mjs) and the contract test
// (src/test/contract/mock-api.contract.test.ts), which checks every
// procedure here against the generated protobuf descriptors.
//
// Handlers take the Connect JSON request body and return the JSON response
// body (protobuf JSON mapping: camelCase fields, enum names, int64 as
// strings, RFC 3339 timestamps). Errors are `RpcError`s carrying the Connect
// code and its HTTP status.

export const ACCESS_TTL_SECONDS = 900;
const REFRESH_TTL_SECONDS = 86_400;
const PAGE_SIZE_MAX = 100;

/** Origin the mock is reachable at; presigned upload/download URLs point here. */
export const MOCK_ORIGIN = `http://127.0.0.1:${process.env.MOCK_API_PORT ?? 8181}`;

export class RpcError extends Error {
  constructor(status, code, message) {
    super(message);
    this.status = status;
    this.code = code;
  }
}
const unauthenticated = (message = "missing or invalid token") =>
  new RpcError(401, "unauthenticated", message);
const notFound = (what) => new RpcError(404, "not_found", `${what} not found`);
const invalid = (message) => new RpcError(400, "invalid_argument", message);
const forbidden = () =>
  new RpcError(403, "permission_denied", "admin role required");

export function createState() {
  return {
    /** email -> user */
    users: new Map(),
    /** access token -> userId */
    accessTokens: new Map(),
    /** refresh token -> userId */
    refreshTokens: new Map(),
    /** media id -> media record (incl. ownerId, bytes) */
    media: new Map(),
    /** push device id -> { userId, ... } */
    devices: new Map(),
    audit: [],
    sequence: 0,
  };
}

export const state = createState();

function nextId() {
  state.sequence += 1;
  return `00000000-0000-4000-8000-${String(state.sequence).padStart(12, "0")}`;
}

const b64url = (value) =>
  Buffer.from(JSON.stringify(value)).toString("base64url");

function issueTokens(user) {
  const now = Math.floor(Date.now() / 1000);
  const accessToken = `${b64url({ alg: "none" })}.${b64url({
    sub: user.id,
    exp: now + ACCESS_TTL_SECONDS,
    roles: user.roles,
    jti: nextId(),
  })}.mock`;
  const refreshToken = `refresh-${nextId()}`;
  state.accessTokens.set(accessToken, user.id);
  state.refreshTokens.set(refreshToken, user.id);
  return {
    accessToken,
    refreshToken,
    accessTokenExpiresInSeconds: String(ACCESS_TTL_SECONDS),
    refreshTokenExpiresInSeconds: String(REFRESH_TTL_SECONDS),
  };
}

const STATUS = {
  TODO: "TASK_STATUS_TODO",
  IN_PROGRESS: "TASK_STATUS_IN_PROGRESS",
  DONE: "TASK_STATUS_DONE",
};
const STATUS_NUMBER = {
  TASK_STATUS_TODO: 1,
  TASK_STATUS_IN_PROGRESS: 2,
  TASK_STATUS_DONE: 3,
};
const PRIORITY = {
  LOW: "TASK_PRIORITY_LOW",
  MEDIUM: "TASK_PRIORITY_MEDIUM",
  HIGH: "TASK_PRIORITY_HIGH",
  URGENT: "TASK_PRIORITY_URGENT",
};

function seedTasks(userId, count) {
  const statuses = [STATUS.TODO, STATUS.IN_PROGRESS, STATUS.DONE];
  const priorities = Object.values(PRIORITY);
  const base = Date.parse("2026-01-01T00:00:00Z");
  return Array.from({ length: count }, (_, index) => {
    const created = new Date(base + index * 60_000).toISOString();
    return {
      id: nextId(),
      title: `Seed task ${String(index + 1).padStart(2, "0")}`,
      description: "",
      status: statuses[index % 3],
      priority: priorities[index % 4],
      // Every 5th task is overdue and open; others have no due date.
      ...(index % 5 === 0 && statuses[index % 3] !== STATUS.DONE
        ? { dueDate: "2020-01-01T00:00:00Z" }
        : {}),
      createdAt: created,
      updatedAt: created,
      createdBy: userId,
      version: 1,
    };
  });
}

function newUser({ email, password, roles = ["user"], tasks = 0 }) {
  const user = {
    id: nextId(),
    email,
    password,
    roles,
    tasks: [],
    notifications: [],
    preferences: {
      pushEnabled: true,
      taskCompleted: true,
      taskDueSoon: true,
      mediaReady: true,
    },
    createdAt: new Date().toISOString(),
  };
  user.tasks = seedTasks(user.id, tasks);
  state.users.set(email, user);
  return user;
}

function userById(id) {
  return [...state.users.values()].find((candidate) => candidate.id === id);
}

function authenticate(req) {
  const header = req.headers.authorization ?? "";
  const token = header.startsWith("Bearer ") ? header.slice(7) : "";
  const user = userById(state.accessTokens.get(token));
  if (!user) {
    throw unauthenticated();
  }
  return user;
}

function requireAdmin(req) {
  const user = authenticate(req);
  if (!user.roles.includes("admin")) {
    throw forbidden();
  }
  return user;
}

const iso = (date) => (date ? date : undefined);

function publicTask(task) {
  return {
    id: task.id,
    title: task.title,
    description: task.description,
    status: task.status,
    priority: task.priority,
    dueDate: iso(task.dueDate),
    createdAt: task.createdAt,
    updatedAt: task.updatedAt,
    completedAt: iso(task.completedAt),
    createdBy: task.createdBy,
    version: task.version,
  };
}

function findTask(user, id) {
  const task = user.tasks.find((candidate) => candidate.id === id);
  if (!task) {
    throw notFound("task");
  }
  return task;
}

function decodePageToken(token) {
  if (!token) return 0;
  const offset = Number(Buffer.from(token, "base64url").toString().slice(2));
  return Number.isFinite(offset) ? offset : 0;
}

/** Offset paging behind opaque tokens, like the real keyset tokens. */
function paginate(rows, body, defaultSize = 20, max = PAGE_SIZE_MAX) {
  const size = Math.min(body.pageSize || defaultSize, max);
  const offset = decodePageToken(body.pageToken);
  const next = offset + size;
  return {
    page: rows.slice(offset, offset + size),
    nextPageToken:
      next < rows.length ? Buffer.from(`o:${next}`).toString("base64url") : "",
  };
}

function notify(user, { kind, title, body = "", link = "", subjectId = "" }) {
  user.notifications.unshift({
    id: nextId(),
    kind,
    title,
    body,
    link,
    subjectId,
    createdAt: new Date().toISOString(),
  });
}

const auth = {
  Register(body) {
    const { email, password } = body;
    if (state.users.has(email)) {
      throw new RpcError(409, "already_exists", "email already registered");
    }
    if (!password || password.length < 8) {
      throw invalid("password too short");
    }
    const user = newUser({ email, password });
    return {
      user: { id: user.id, email, createdAt: user.createdAt },
      tokens: issueTokens(user),
    };
  },
  Login(body) {
    const user = state.users.get(body.email);
    if (!user || user.password !== body.password) {
      throw unauthenticated("invalid credentials");
    }
    return { userId: user.id, tokens: issueTokens(user) };
  },
  RefreshToken(body) {
    const user = userById(state.refreshTokens.get(body.refreshToken));
    if (!user) {
      throw unauthenticated("refresh token revoked or unknown");
    }
    // Rotation: the presented token is single-use.
    state.refreshTokens.delete(body.refreshToken);
    return { tokens: issueTokens(user) };
  },
  Logout(body) {
    state.refreshTokens.delete(body.refreshToken);
    return {};
  },
  GetCurrentUser(_body, req) {
    const user = authenticate(req);
    return {
      user: { id: user.id, email: user.email, createdAt: user.createdAt },
    };
  },
};

const tasks = {
  CreateTask(body, req) {
    const user = authenticate(req);
    if (!body.title) {
      throw invalid("title is required");
    }
    const now = new Date().toISOString();
    const task = {
      id: nextId(),
      title: body.title,
      description: body.description ?? "",
      status: STATUS.TODO,
      priority: body.priority ?? PRIORITY.MEDIUM,
      dueDate: body.dueDate,
      createdAt: now,
      updatedAt: now,
      createdBy: user.id,
      version: 1,
    };
    user.tasks.unshift(task);
    return { task: publicTask(task) };
  },
  GetTask(body, req) {
    return { task: publicTask(findTask(authenticate(req), body.id)) };
  },
  ListTasks(body, req) {
    const user = authenticate(req);
    const statuses = body.statuses ?? [];
    const priorities = body.priorities ?? [];
    const search = (body.search ?? "").toLowerCase();

    let rows = user.tasks.filter(
      (task) =>
        (statuses.length === 0 || statuses.includes(task.status)) &&
        (priorities.length === 0 || priorities.includes(task.priority)) &&
        (search === "" || task.title.toLowerCase().includes(search)),
    );

    const byDue = body.sortBy === "TASK_SORT_FIELD_DUE_DATE";
    const direction = body.sortDirection === "SORT_DIRECTION_ASC" ? 1 : -1;
    const key = (task) =>
      Date.parse(byDue ? (task.dueDate ?? "9999-01-01") : task.createdAt);
    rows = [...rows].sort((a, b) => (key(a) - key(b)) * direction || 0);

    const { page, nextPageToken } = paginate(rows, body);
    return {
      tasks: page.map(publicTask),
      nextPageToken,
      totalCount: rows.length,
    };
  },
  UpdateTask(body, req) {
    const task = findTask(authenticate(req), body.id);
    for (const path of body.updateMask?.paths ??
      String(body.updateMask ?? "")
        .split(",")
        .filter(Boolean)) {
      if (path === "title") task.title = body.title;
      if (path === "description") task.description = body.description;
      if (path === "priority") task.priority = body.priority;
      if (path === "due_date" || path === "dueDate")
        task.dueDate = body.dueDate;
    }
    task.version += 1;
    task.updatedAt = new Date().toISOString();
    return { task: publicTask(task) };
  },
  DeleteTask(body, req) {
    const user = authenticate(req);
    const task = findTask(user, body.id);
    user.tasks.splice(user.tasks.indexOf(task), 1);
    return {};
  },
  CompleteTask(body, req) {
    const user = authenticate(req);
    const task = findTask(user, body.id);
    if (task.status !== STATUS.DONE) {
      // The real server derives this from TaskCompleted via the outbox.
      notify(user, {
        kind: "NOTIFICATION_KIND_TASK_COMPLETED",
        title: "Task completed",
        body: task.title,
        link: `/tasks/${task.id}`,
        subjectId: task.id,
      });
    }
    task.status = STATUS.DONE;
    task.completedAt = new Date().toISOString();
    task.updatedAt = task.completedAt;
    return { task: publicTask(task) };
  },
  ReopenTask(body, req) {
    const task = findTask(authenticate(req), body.id);
    task.status = STATUS.TODO;
    task.completedAt = undefined;
    task.updatedAt = new Date().toISOString();
    return { task: publicTask(task) };
  },
  GetTaskStats(_body, req) {
    const user = authenticate(req);
    const now = Date.now();
    const count = (predicate) => user.tasks.filter(predicate).length;
    return {
      total: user.tasks.length,
      todo: count((t) => t.status === STATUS.TODO),
      inProgress: count((t) => t.status === STATUS.IN_PROGRESS),
      done: count((t) => t.status === STATUS.DONE),
      overdue: count(
        (t) =>
          t.status !== STATUS.DONE && t.dueDate && Date.parse(t.dueDate) < now,
      ),
    };
  },
};

const HL_START = "";
const HL_END = "";

/** Wraps case-insensitive occurrences of `term` in the highlight markers. */
function highlight(text, term) {
  if (!term) return text;
  const lower = text.toLowerCase();
  let out = "";
  let at = 0;
  for (;;) {
    const found = lower.indexOf(term, at);
    if (found === -1) break;
    out += `${text.slice(at, found)}${HL_START}${text.slice(found, found + term.length)}${HL_END}`;
    at = found + term.length;
  }
  return out + text.slice(at);
}

const search = {
  Search(body, req) {
    const user = authenticate(req);
    const term = String(body.query ?? "")
      .trim()
      .toLowerCase();
    if (!term) {
      throw invalid("query is required");
    }
    const kinds = body.kinds ?? [];
    const wantTasks =
      kinds.length === 0 || kinds.includes("DOCUMENT_KIND_TASK");
    const wantMedia =
      kinds.length === 0 || kinds.includes("DOCUMENT_KIND_MEDIA");
    const hits = [];
    if (wantTasks) {
      for (const task of user.tasks) {
        const haystack = `${task.title} ${task.description}`.toLowerCase();
        if (haystack.includes(term)) {
          hits.push({
            kind: "DOCUMENT_KIND_TASK",
            id: task.id,
            title: highlight(task.title, term),
            snippet: highlight(task.description || task.title, term),
            score: 1,
            link: `/tasks/${task.id}`,
            updatedAt: task.updatedAt,
          });
        }
      }
    }
    if (wantMedia) {
      for (const media of state.media.values()) {
        if (
          media.ownerId === user.id &&
          media.filename.toLowerCase().includes(term)
        ) {
          hits.push({
            kind: "DOCUMENT_KIND_MEDIA",
            id: media.id,
            title: highlight(media.filename, term),
            snippet: "",
            score: 0.5,
            link: media.taskId ? `/tasks/${media.taskId}` : "/search",
            updatedAt: media.createdAt,
          });
        }
      }
    }
    const { page, nextPageToken } = paginate(hits, body, 20, 50);
    return {
      hits: page,
      nextPageToken,
      totalEstimate: body.pageToken ? 0 : hits.length,
    };
  },
  Suggest(body, req) {
    const user = authenticate(req);
    const prefix = String(body.prefix ?? "").toLowerCase();
    const limit = Math.min(body.limit || 8, 20);
    return {
      suggestions: user.tasks
        .map((task) => task.title)
        .filter((title) => title.toLowerCase().startsWith(prefix))
        .slice(0, limit),
    };
  },
};

function publicNotification(n) {
  return {
    id: n.id,
    kind: n.kind,
    title: n.title,
    body: n.body,
    link: n.link,
    subjectId: n.subjectId,
    createdAt: n.createdAt,
    readAt: n.readAt,
  };
}

const notifications = {
  ListNotifications(body, req) {
    const user = authenticate(req);
    const rows = body.unreadOnly
      ? user.notifications.filter((n) => !n.readAt)
      : user.notifications;
    const { page, nextPageToken } = paginate(rows, body);
    return { notifications: page.map(publicNotification), nextPageToken };
  },
  GetUnreadCount(_body, req) {
    const user = authenticate(req);
    return { unread: user.notifications.filter((n) => !n.readAt).length };
  },
  MarkRead(body, req) {
    const user = authenticate(req);
    const ids = new Set(body.ids ?? []);
    if (ids.size === 0) throw invalid("ids is required");
    const now = new Date().toISOString();
    for (const n of user.notifications) {
      if (ids.has(n.id) && !n.readAt) n.readAt = now;
    }
    return {};
  },
  MarkAllRead(_body, req) {
    const user = authenticate(req);
    const now = new Date().toISOString();
    let marked = 0;
    for (const n of user.notifications) {
      if (!n.readAt) {
        n.readAt = now;
        marked += 1;
      }
    }
    return { marked };
  },
  RegisterPushDevice(body, req) {
    const user = authenticate(req);
    if (!body.platform || body.platform === "PUSH_PLATFORM_UNSPECIFIED") {
      throw invalid("platform is required");
    }
    if (body.platform === "PUSH_PLATFORM_WEB_PUSH" && !body.webPush?.endpoint) {
      throw invalid("web_push is required for WEB_PUSH");
    }
    const deviceId = nextId();
    state.devices.set(deviceId, { userId: user.id, ...body });
    return { deviceId };
  },
  UnregisterPushDevice(body, req) {
    const user = authenticate(req);
    const device = state.devices.get(body.deviceId);
    if (!device || device.userId !== user.id) throw notFound("device");
    state.devices.delete(body.deviceId);
    return {};
  },
  GetWebPushConfig(_body, req) {
    authenticate(req);
    // Not configured in the mock: the UI shows the "not set up" state.
    return { vapidPublicKey: process.env.MOCK_VAPID_PUBLIC_KEY ?? "" };
  },
  GetPreferences(_body, req) {
    return { preferences: { ...authenticate(req).preferences } };
  },
  UpdatePreferences(body, req) {
    const user = authenticate(req);
    if (!body.preferences) throw invalid("preferences is required");
    user.preferences = {
      pushEnabled: Boolean(body.preferences.pushEnabled),
      taskCompleted: Boolean(body.preferences.taskCompleted),
      taskDueSoon: Boolean(body.preferences.taskDueSoon),
      mediaReady: Boolean(body.preferences.mediaReady),
    };
    return { preferences: { ...user.preferences } };
  },
};

function mediaKind(contentType) {
  if (contentType.startsWith("image/")) return "MEDIA_KIND_IMAGE";
  if (contentType.startsWith("video/")) return "MEDIA_KIND_VIDEO";
  if (contentType.startsWith("audio/")) return "MEDIA_KIND_AUDIO";
  return "MEDIA_KIND_DOCUMENT";
}

function publicMedia(m) {
  const ready = m.status === "MEDIA_STATUS_READY";
  const objectUrl = `${MOCK_ORIGIN}/__storage/${m.id}`;
  return {
    id: m.id,
    kind: m.kind,
    status: m.status,
    filename: m.filename,
    contentType: m.contentType,
    sizeBytes: String(m.sizeBytes),
    url: ready ? objectUrl : "",
    // The mock "processes" images by serving the original as every rendition.
    variants:
      ready && m.kind === "MEDIA_KIND_IMAGE"
        ? [
            {
              kind: "VARIANT_KIND_THUMBNAIL",
              url: objectUrl,
              contentType: m.contentType,
              width: 320,
              height: 320,
              sizeBytes: String(m.sizeBytes),
            },
          ]
        : [],
    taskId: m.taskId || undefined,
    createdAt: m.createdAt,
  };
}

function ownedMedia(user, id) {
  const media = state.media.get(id);
  if (!media || media.ownerId !== user.id) throw notFound("media");
  return media;
}

const MAX_UPLOAD_BYTES = 50 * 1024 * 1024;

const media = {
  CreateUpload(body, req) {
    const user = authenticate(req);
    const size = Number(body.sizeBytes ?? 0);
    if (!body.filename || !body.contentType || size <= 0) {
      throw invalid("filename, content_type and size_bytes are required");
    }
    if (size > MAX_UPLOAD_BYTES) {
      throw new RpcError(429, "resource_exhausted", "file too large");
    }
    if (body.taskId) findTask(user, body.taskId);
    const record = {
      id: nextId(),
      ownerId: user.id,
      kind: mediaKind(body.contentType),
      status: "MEDIA_STATUS_PENDING_UPLOAD",
      filename: body.filename,
      contentType: body.contentType,
      sizeBytes: size,
      taskId: body.taskId ?? "",
      createdAt: new Date().toISOString(),
      bytes: null,
    };
    state.media.set(record.id, record);
    return {
      media: publicMedia(record),
      uploadUrl: `${MOCK_ORIGIN}/__storage/${record.id}`,
      method: "PUT",
      headers: { "Content-Type": body.contentType },
      expiresAt: new Date(Date.now() + 15 * 60_000).toISOString(),
    };
  },
  CompleteUpload(body, req) {
    const record = ownedMedia(authenticate(req), body.id);
    if (!record.bytes) {
      throw new RpcError(400, "failed_precondition", "object not uploaded");
    }
    record.status = "MEDIA_STATUS_READY";
    return { media: publicMedia(record) };
  },
  GetMedia(body, req) {
    return { media: publicMedia(ownedMedia(authenticate(req), body.id)) };
  },
  ListMedia(body, req) {
    const user = authenticate(req);
    const kinds = body.kinds ?? [];
    const rows = [...state.media.values()]
      .filter(
        (m) =>
          m.ownerId === user.id &&
          m.status !== "MEDIA_STATUS_PENDING_UPLOAD" &&
          (!body.taskId || m.taskId === body.taskId) &&
          (kinds.length === 0 || kinds.includes(m.kind)),
      )
      .reverse();
    const { page, nextPageToken } = paginate(rows, body);
    return { media: page.map(publicMedia), nextPageToken };
  },
  DeleteMedia(body, req) {
    const record = ownedMedia(authenticate(req), body.id);
    state.media.delete(record.id);
    return {};
  },
};

function audit(actor, action, targetKind, targetId, reason = "") {
  state.audit.unshift({
    id: nextId(),
    actorId: actor.id,
    action,
    targetKind,
    targetId,
    reason,
    createdAt: new Date().toISOString(),
  });
}

const admin = {
  GetOverview(_body, req) {
    requireAdmin(req);
    const users = [...state.users.values()];
    const allTasks = users.flatMap((u) => u.tasks);
    const now = Date.now();
    return {
      usersTotal: String(users.length),
      tasksTotal: String(allTasks.length),
      tasksDone: String(
        allTasks.filter((t) => t.status === STATUS.DONE).length,
      ),
      tasksOverdue: String(
        allTasks.filter(
          (t) =>
            t.status !== STATUS.DONE &&
            t.dueDate &&
            Date.parse(t.dueDate) < now,
        ).length,
      ),
      mediaTotal: String(state.media.size),
      mediaBytes: String(
        [...state.media.values()].reduce((sum, m) => sum + m.sizeBytes, 0),
      ),
      pushDevices: String(state.devices.size),
      outboxLagSeconds: "0",
      jobsPending: "0",
    };
  },
  ListUsers(body, req) {
    requireAdmin(req);
    const q = String(body.query ?? "").toLowerCase();
    const rows = [...state.users.values()].filter(
      (u) => !q || u.email.toLowerCase().includes(q),
    );
    const { page, nextPageToken } = paginate(rows, body, 25);
    return {
      users: page.map((u) => ({
        id: u.id,
        email: u.email,
        displayName: "",
        createdAt: u.createdAt,
        taskCount: String(u.tasks.length),
      })),
      nextPageToken,
    };
  },
  ListTasks(body, req) {
    requireAdmin(req);
    const q = String(body.query ?? "").toLowerCase();
    const rows = [...state.users.values()]
      .filter((u) => !body.ownerId || u.id === body.ownerId)
      .flatMap((u) => u.tasks)
      .filter((t) => !q || t.title.toLowerCase().includes(q));
    const { page, nextPageToken } = paginate(rows, body, 25);
    return {
      tasks: page.map((t) => ({
        id: t.id,
        ownerId: t.createdBy,
        title: t.title,
        status: STATUS_NUMBER[t.status] ?? 0,
        createdAt: t.createdAt,
        dueDate: iso(t.dueDate),
      })),
      nextPageToken,
    };
  },
  DeleteTask(body, req) {
    const actor = requireAdmin(req);
    const reason = String(body.reason ?? "").trim();
    if (reason.length < 3)
      throw invalid("reason must be at least 3 characters");
    for (const user of state.users.values()) {
      const task = user.tasks.find((t) => t.id === body.id);
      if (task) {
        user.tasks.splice(user.tasks.indexOf(task), 1);
        audit(actor, "task.delete", "task", task.id, reason);
        return {};
      }
    }
    throw notFound("task");
  },
  BroadcastNotification(body, req) {
    const actor = requireAdmin(req);
    if (!body.title) throw invalid("title is required");
    const recipients = body.userId
      ? [userById(body.userId)].filter(Boolean)
      : [...state.users.values()];
    for (const user of recipients) {
      notify(user, {
        kind: "NOTIFICATION_KIND_SYSTEM",
        title: body.title,
        body: body.body ?? "",
        link: body.link ?? "",
      });
    }
    audit(actor, "notification.broadcast", "user", body.userId ?? "");
    return { recipients: String(recipients.length) };
  },
  ListAuditLog(body, req) {
    requireAdmin(req);
    const { page, nextPageToken } = paginate(state.audit, body, 50);
    return { entries: page, nextPageToken };
  },
};

/** Every mocked Connect service, keyed by fully-qualified service name. */
export const services = {
  "aedst.auth.v1.AuthService": auth,
  "aedst.tasks.v1.TasksService": tasks,
  "aedst.search.v1.SearchService": search,
  "aedst.notifications.v1.NotificationsService": notifications,
  "aedst.media.v1.MediaService": media,
  "aedst.admin.v1.AdminService": admin,
};

/** Test control plane — never exposed by the real server. */
export const control = {
  // Creates an isolated user with `tasks` seeded tasks (and optional roles,
  // e.g. ["user", "admin"], and seeded notifications).
  "/__test/seed"(body) {
    const user = newUser({
      email: body.email,
      password: body.password ?? "password123",
      roles: body.roles ?? ["user"],
      tasks: body.tasks ?? 25,
    });
    for (let i = 0; i < (body.notifications ?? 0); i += 1) {
      notify(user, {
        kind: "NOTIFICATION_KIND_SYSTEM",
        title: `Seed notification ${i + 1}`,
        body: "Hello from the mock API",
        link: "/tasks",
      });
    }
    return { email: user.email, password: user.password, userId: user.id };
  },
  // Every access token of the user stops working; refresh tokens still do —
  // simulates an access token expiring while the session lives on.
  "/__test/expire-access"(body) {
    const user = state.users.get(body.email);
    for (const [token, userId] of state.accessTokens) {
      if (userId === user?.id) state.accessTokens.delete(token);
    }
    return {};
  },
  // Everything is revoked — simulates a session ended server-side.
  "/__test/revoke-session"(body) {
    const user = state.users.get(body.email);
    for (const map of [state.accessTokens, state.refreshTokens]) {
      for (const [token, userId] of map) {
        if (userId === user?.id) map.delete(token);
      }
    }
    return {};
  },
};

/** Presigned-URL stand-in: PUT stores the object, GET serves it. */
export function storeObject(id, bytes) {
  const record = state.media.get(id);
  if (!record) return false;
  record.bytes = bytes;
  return true;
}

export function readObject(id) {
  const record = state.media.get(id);
  return record?.bytes
    ? { bytes: record.bytes, type: record.contentType }
    : null;
}
