"use client";

import { useQuery } from "@connectrpc/connect-query";
import { skipToken } from "@tanstack/react-query";
import { useTranslations } from "next-intl";
import { useEffect, useState } from "react";

import { useRouter } from "@/app/config/i18n/navigation";
import { useCommandPalette } from "@/modules/search/lib/command-palette-store";
import { normalizeQuery } from "@/modules/search/lib/search-params";
import { HighlightedText } from "@/modules/search/ui/HighlightedText";
import { SearchService } from "@/shared/api/connect/client";
import { track } from "@/shared/lib/analytics/analytics";
import { useFeatureFlag } from "@/shared/lib/analytics/use-feature-flag";
import { useDebouncedValue } from "@/shared/lib/hooks/use-debounced-value";
import {
  Command,
  CommandDialog,
  CommandEmpty,
  CommandGroup,
  CommandInput,
  CommandItem,
  CommandList,
} from "@/shared/ui/command";
import { Kbd } from "@/shared/ui/kbd";

const DEBOUNCE_MS = 200;

const ACTIONS = [
  { href: "/tasks/new", key: "newTask" },
  { href: "/tasks", key: "goTasks" },
  { href: "/notifications", key: "goNotifications" },
] as const;

/**
 * ⌘K / Ctrl+K palette (cmdk via shared/ui/command): quick actions, plus live
 * SearchService results for what is typed. Filtering is the server's job,
 * so cmdk's own fuzzy filter is off (`shouldFilter={false}`). Behind the
 * `command-palette` flag (default on) as the reference flag-gated feature.
 */
export function CommandPalette() {
  const t = useTranslations("search.palette");
  const enabled = useFeatureFlag("command-palette");
  const router = useRouter();
  const { open, setOpen, toggle } = useCommandPalette();
  const [input, setInput] = useState("");
  const query = normalizeQuery(useDebouncedValue(input, DEBOUNCE_MS));

  useEffect(() => {
    if (!enabled) {
      return;
    }
    const onKeyDown = (event: KeyboardEvent) => {
      if (event.key.toLowerCase() === "k" && (event.metaKey || event.ctrlKey)) {
        event.preventDefault();
        if (!useCommandPalette.getState().open) {
          track("command_palette_opened", { via: "shortcut" });
        }
        toggle();
      }
    };
    window.addEventListener("keydown", onKeyDown);
    return () => window.removeEventListener("keydown", onKeyDown);
  }, [enabled, toggle]);

  const results = useQuery(
    SearchService.method.search,
    open && query ? { query, pageSize: 8 } : skipToken,
    { meta: { feature: "search" } },
  );

  if (!enabled) {
    return null;
  }

  function go(href: string) {
    setOpen(false);
    setInput("");
    router.push(href);
  }

  return (
    <CommandDialog
      open={open}
      onOpenChange={setOpen}
      title={t("title")}
      description={t("description")}
    >
      <Command shouldFilter={false}>
        <CommandInput
          value={input}
          onValueChange={setInput}
          placeholder={t("placeholder")}
        />
        <CommandList>
          <CommandEmpty>
            {results.isFetching ? t("searching") : t("noResults")}
          </CommandEmpty>
          {(results.data?.hits.length ?? 0) > 0 && (
            <CommandGroup heading={t("results")}>
              {results.data?.hits.map((hit) => (
                <CommandItem
                  key={`${hit.kind}:${hit.id}`}
                  value={`hit:${hit.kind}:${hit.id}`}
                  onSelect={() => go(hit.link || "/search")}
                >
                  <HighlightedText text={hit.title} />
                </CommandItem>
              ))}
            </CommandGroup>
          )}
          {query && (
            <CommandGroup>
              <CommandItem
                value="search-all"
                onSelect={() => go(`/search?q=${encodeURIComponent(query)}`)}
              >
                {t("searchAll", { query })}
              </CommandItem>
            </CommandGroup>
          )}
          <CommandGroup heading={t("actions")}>
            {ACTIONS.map((action) => (
              <CommandItem
                key={action.key}
                value={`action:${action.key}`}
                onSelect={() => go(action.href)}
              >
                {t(action.key)}
              </CommandItem>
            ))}
          </CommandGroup>
        </CommandList>
      </Command>
    </CommandDialog>
  );
}

/** Header button that opens the palette, showing the shortcut. */
export function CommandPaletteTrigger() {
  const t = useTranslations("search.palette");
  const enabled = useFeatureFlag("command-palette");
  const setOpen = useCommandPalette((state) => state.setOpen);
  if (!enabled) {
    return null;
  }
  return (
    <button
      type="button"
      onClick={() => {
        track("command_palette_opened", { via: "button" });
        setOpen(true);
      }}
      className="flex items-center gap-2 rounded-md border px-3 py-1.5 text-muted-foreground text-sm hover:bg-muted"
    >
      {t("open")}
      {/* text-foreground: the default muted-on-muted Kbd fails WCAG contrast. */}
      <Kbd className="text-foreground">Ctrl K</Kbd>
    </button>
  );
}
