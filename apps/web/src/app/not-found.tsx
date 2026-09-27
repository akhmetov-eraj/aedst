export default function RootNotFound() {
  return (
    <html lang="en">
      <body className="flex min-h-screen flex-col items-center justify-center gap-4 px-4 text-center font-sans">
        <p className="text-sm font-medium text-neutral-500">404</p>
        <h1 className="text-2xl font-semibold text-neutral-900">
          Page not found
        </h1>
        <p className="max-w-md text-sm text-neutral-500">
          The page you’re looking for doesn’t exist or has been moved.
        </p>
        <a
          href="/"
          className="text-sm font-medium text-neutral-900 underline underline-offset-4"
        >
          Go home
        </a>
      </body>
    </html>
  );
}
