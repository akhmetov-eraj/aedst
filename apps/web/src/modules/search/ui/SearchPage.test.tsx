import { screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { describe, expect, it } from "vitest";

import { SearchPage } from "@/modules/search/ui/SearchPage";
import {
  DocumentKind,
  SearchService,
} from "@/shared/api/connect/generated/aedst/search/v1/search_pb";
import { rpcErrorHandler, rpcHandler } from "@/test/mocks/connect";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.search;

describe("SearchPage", () => {
  it("prompts before anything is typed and sends no request", () => {
    renderWithProviders(<SearchPage />);
    expect(screen.getByText(t.emptyPrompt)).toBeInTheDocument();
  });

  it("searches the URL query, highlights matches and keys kinds to the filter", async () => {
    const requests: { query: string; kinds: DocumentKind[] }[] = [];
    server.use(
      rpcHandler(SearchService.method.search, (input) => {
        requests.push({ query: input.query, kinds: input.kinds });
        return {
          hits: [
            {
              kind: DocumentKind.TASK,
              id: "t-1",
              title: "Write docs",
              snippet: "all the docs",
              link: "/tasks/t-1",
            },
          ],
          totalEstimate: 1,
        };
      }),
    );

    renderWithProviders(<SearchPage />, { searchParams: "?q=docs&kind=task" });

    expect(await screen.findByText("1 result")).toBeInTheDocument();
    const link = screen.getByRole("link");
    expect(link).toHaveAttribute("href", "/tasks/t-1");
    expect(link).toHaveTextContent("Write docs");
    expect(link.querySelectorAll("mark")).toHaveLength(2);
    expect(requests[0]).toEqual({ query: "docs", kinds: [DocumentKind.TASK] });
  });

  it("debounces typing into one request and writes the query to the URL", async () => {
    let calls = 0;
    server.use(
      rpcHandler(SearchService.method.search, () => {
        calls += 1;
        return { hits: [] };
      }),
    );
    const urls: string[] = [];
    renderWithProviders(<SearchPage />, {
      onUrlUpdate: ({ queryString }) => urls.push(queryString),
    });

    await userEvent.type(screen.getByLabelText(t.label), "report");

    expect(await screen.findByText(t.noResults)).toBeInTheDocument();
    expect(calls).toBe(1);
    expect(urls.at(-1)).toContain("q=report");
  });

  it("offers a retry when search fails", async () => {
    server.use(rpcErrorHandler(SearchService.method.search, "internal", "x"));
    renderWithProviders(<SearchPage />, { searchParams: "?q=x" });
    expect(await screen.findByRole("alert")).toHaveTextContent(t.errorTitle);
  });
});
