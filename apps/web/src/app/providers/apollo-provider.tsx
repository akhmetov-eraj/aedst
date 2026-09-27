"use client";

import {
  ApolloClient,
  ApolloNextAppProvider,
  InMemoryCache,
} from "@apollo/client-integration-nextjs";

import { makeHttpLink } from "@/app/config/apollo/links";

function makeClient() {
  return new ApolloClient({
    cache: new InMemoryCache(),
    link: makeHttpLink(),
  });
}

export function ApolloProvider({
  children,
}: Readonly<{
  children: React.ReactNode;
}>) {
  return (
    <ApolloNextAppProvider makeClient={makeClient}>
      {children}
    </ApolloNextAppProvider>
  );
}
