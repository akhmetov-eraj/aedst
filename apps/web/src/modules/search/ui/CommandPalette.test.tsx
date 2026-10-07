import { act, screen } from "@testing-library/react";
import userEvent from "@testing-library/user-event";
import { beforeEach, describe, expect, it } from "vitest";

import { useCommandPalette } from "@/modules/search/lib/command-palette-store";
import {
  CommandPalette,
  CommandPaletteTrigger,
} from "@/modules/search/ui/CommandPalette";
import {
  DocumentKind,
  SearchService,
} from "@/shared/api/connect/generated/aedst/search/v1/search_pb";
import { rpcHandler } from "@/test/mocks/connect";
import { routerMock } from "@/test/mocks/navigation";
import { server } from "@/test/mocks/server";
import { messages, renderWithProviders } from "@/test/render";

const t = messages.search.palette;

beforeEach(() => {
  act(() => useCommandPalette.setState({ open: false }));
});

describe("CommandPalette", () => {
  it("opens from the trigger and runs quick actions", async () => {
    renderWithProviders(
      <>
        <CommandPaletteTrigger />
        <CommandPalette />
      </>,
    );
    await userEvent.click(screen.getByRole("button", { name: /Search…/ }));
    await userEvent.click(
      await screen.findByRole("option", { name: t.newTask }),
    );
    expect(routerMock.push).toHaveBeenCalledWith("/tasks/new");
    expect(useCommandPalette.getState().open).toBe(false);
  });

  it("toggles with Ctrl+K and jumps to a live search hit", async () => {
    server.use(
      rpcHandler(SearchService.method.search, (input) => ({
        hits: [
          {
            kind: DocumentKind.TASK,
            id: "t-9",
            title: `${input.query} report`,
            link: "/tasks/t-9",
          },
        ],
      })),
    );
    renderWithProviders(<CommandPalette />);

    await userEvent.keyboard("{Control>}k{/Control}");
    const input = await screen.findByPlaceholderText(t.placeholder);
    await userEvent.type(input, "q3");
    await userEvent.click(
      await screen.findByRole("option", { name: /report/ }, { timeout: 3_000 }),
    );
    expect(routerMock.push).toHaveBeenCalledWith("/tasks/t-9");
  });

  it("offers a full search for the typed text", async () => {
    server.use(rpcHandler(SearchService.method.search, () => ({ hits: [] })));
    renderWithProviders(<CommandPalette />);
    act(() => useCommandPalette.setState({ open: true }));

    await userEvent.type(
      await screen.findByPlaceholderText(t.placeholder),
      "budget",
    );
    await userEvent.click(
      await screen.findByRole("option", { name: "Search for “budget”" }),
    );
    expect(routerMock.push).toHaveBeenCalledWith("/search?q=budget");
  });
});
