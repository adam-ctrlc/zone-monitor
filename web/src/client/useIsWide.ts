import { useSyncExternalStore } from "react";

const WIDE_SCREEN = "(min-width: 1024px) and (min-height: 540px)";

const subscribe = (onChange: () => void): (() => void) => {
  const query = window.matchMedia(WIDE_SCREEN);
  query.addEventListener("change", onChange);
  return () => query.removeEventListener("change", onChange);
};

// True when there is room for every panel at once.
export function useIsWide(): boolean {
  return useSyncExternalStore(subscribe, () => window.matchMedia(WIDE_SCREEN).matches);
}
