import { act, renderHook } from "@testing-library/react";
import { describe, expect, it } from "vitest";

import { useCursorPages } from "@/modules/admin/lib/use-cursor-pages";

describe("useCursorPages", () => {
  it("pushes next tokens, pops on previous, ignores an empty token, resets", () => {
    const { result } = renderHook(() => useCursorPages());
    expect(result.current).toMatchObject({
      pageToken: "",
      page: 1,
      hasPrevious: false,
    });

    act(() => result.current.next("t2"));
    act(() => result.current.next("t3"));
    act(() => result.current.next(""));
    expect(result.current).toMatchObject({
      pageToken: "t3",
      page: 3,
      hasPrevious: true,
    });

    act(() => result.current.previous());
    expect(result.current.pageToken).toBe("t2");

    act(() => result.current.reset());
    expect(result.current).toMatchObject({ pageToken: "", page: 1 });
  });
});
