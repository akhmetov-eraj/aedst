import { CurrentUser } from "@/modules/auth/ui/current-user";
import { SignOutButton } from "@/modules/auth/ui/sign-out-button";
import { RequireAuth } from "@/shared/lib/auth/require-auth";

export default function TasksLayout({
  children,
}: {
  children: React.ReactNode;
}) {
  return (
    <RequireAuth>
      <div className="mx-auto flex w-full max-w-3xl flex-1 flex-col gap-6 p-4 sm:p-6">
        <header className="flex items-center justify-between gap-3">
          <CurrentUser />
          <SignOutButton />
        </header>
        {children}
      </div>
    </RequireAuth>
  );
}
