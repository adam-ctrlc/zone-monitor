import { useCallback, useState } from "react";

// Runs a server request and reports whether it is busy or failed.
export function useAction() {
  const [busy, setBusy] = useState(false);
  const [error, setError] = useState<string | null>(null);

  const run = useCallback(async (action: () => Promise<void>) => {
    setBusy(true);
    setError(null);
    try {
      await action();
    } catch {
      setError("That did not reach the server. Try again.");
    } finally {
      setBusy(false);
    }
  }, []);

  return { busy, error, run };
}
