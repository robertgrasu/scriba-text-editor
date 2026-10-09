interface User { id: number; name?: string }
export async function load<T>(url: string): Promise<T> {
  const r = await fetch(url, { method: "GET" });
  if (!r.ok) throw new Error(`bad ${r.status}`);
  return (await r.json()) as T;
}
