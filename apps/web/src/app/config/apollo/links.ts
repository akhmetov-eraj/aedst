import { HttpLink } from "@apollo/client";

import { env } from "@/app/config/env";

export function makeHttpLink() {
  return new HttpLink({
    uri: env.NEXT_PUBLIC_GRAPHQL_ENDPOINT,
    fetchOptions: {
      cache: "no-store",
    },
  });
}
