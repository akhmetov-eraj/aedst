import {
  create,
  type DescMessage,
  type DescMethodUnary,
  fromJson,
  type JsonValue,
  type MessageInitShape,
  type MessageShape,
  toJson,
} from "@bufbuild/protobuf";
import { HttpResponse, http } from "msw";

/** Absolute so Node's fetch (which has no page origin under jsdom) accepts it. */
export const TEST_RPC_BASE_URL = "http://localhost/api/rpc";
/** Where server-side code (Server Actions, refresh, proxy) reaches apps/server. */
export const TEST_API_ORIGIN = "http://localhost:8080";

/**
 * MSW handler for one Connect unary RPC, speaking the Connect JSON protocol
 * (`POST /<service>/<method>`), so component tests exercise the real generated
 * client, real (de)serialization and TanStack Query — only the network is fake.
 * `origin` picks the browser-facing proxy path (default) or the API itself.
 */
export function rpcHandler<I extends DescMessage, O extends DescMessage>(
  method: DescMethodUnary<I, O>,
  resolve: (
    input: MessageShape<I>,
    request: Request,
  ) => MessageInitShape<O> | Promise<MessageInitShape<O>>,
  origin: string = TEST_RPC_BASE_URL,
) {
  return http.post(
    `${origin}/${method.parent.typeName}/${method.name}`,
    async ({ request }) => {
      const body = (await request.clone().json()) as JsonValue;
      const output = await resolve(fromJson(method.input, body), request);
      return HttpResponse.json(
        toJson(method.output, create(method.output, output)),
      );
    },
  );
}

/** A Connect error response, e.g. `rpcError(TasksService.method.getTask, "not_found", ...)`. */
export function rpcErrorHandler<I extends DescMessage, O extends DescMessage>(
  method: DescMethodUnary<I, O>,
  code: "unauthenticated" | "not_found" | "invalid_argument" | "internal",
  message: string,
  origin: string = TEST_RPC_BASE_URL,
) {
  const status = {
    unauthenticated: 401,
    not_found: 404,
    invalid_argument: 400,
    internal: 500,
  }[code];

  return http.post(`${origin}/${method.parent.typeName}/${method.name}`, () =>
    HttpResponse.json({ code, message }, { status }),
  );
}
