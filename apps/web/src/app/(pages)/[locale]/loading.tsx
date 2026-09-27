import { Spinner } from "@/shared/ui/spinner";

export default function LocaleLoading() {
  return (
    <div className="flex flex-1 items-center justify-center py-24">
      <Spinner className="size-6" />
    </div>
  );
}
