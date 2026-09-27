import { ofetch } from "ofetch";

import { env } from "@/app/config/env";

/**
 * Shared REST client for use as a `queryFn`/`mutationFn` with TanStack Query
 * (react-query). GraphQL traffic goes through Apollo Client instead — see
 * `@/app/config/apollo`.
 */
export const httpClient = ofetch.create({
  baseURL: env.NEXT_PUBLIC_REST_API_URL,
});
