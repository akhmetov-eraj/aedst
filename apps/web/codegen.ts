import type { CodegenConfig } from "@graphql-codegen/cli";

const config: CodegenConfig = {
  // Placeholder SDL — point this at the server's introspection endpoint
  // (e.g. env.NEXT_PUBLIC_GRAPHQL_ENDPOINT) or a generated SDL file once
  // apps/server ships a real schema.
  schema: "./src/app/config/graphql/schema.graphql",
  documents: ["src/**/*.graphql"],
  generates: {
    "src/shared/api/graphql/generated/types.ts": {
      plugins: ["typescript", "typescript-operations"],
    },
  },
  ignoreNoDocuments: true,
};

export default config;
