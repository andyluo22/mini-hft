import { useEffect, useState } from "react";

export default function Health() {
  const [status, setStatus] = useState("checking...");
  useEffect(() => {
    const base = process.env.NEXT_PUBLIC_API_BASE || "http://localhost:8000";
    fetch(`${base}/health`).then(r => r.json()).then(j => setStatus(j.status))
      .catch(() => setStatus("error"));
  }, []);
  return (
    <main className="min-h-screen flex items-center justify-center">
      <div className="p-6 rounded-xl border">
        <h1 className="text-2xl font-semibold mb-2">Web : API Health</h1>
        <p>API status: <span className="font-mono">{status}</span></p>
        <p className="text-sm text-gray-500 mt-2">Set NEXT_PUBLIC_API_BASE to point at the API.</p>
      </div>
    </main>
  );
}