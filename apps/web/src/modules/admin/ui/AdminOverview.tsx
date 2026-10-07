"use client";

import { useQuery } from "@connectrpc/connect-query";
import { useLocale, useTranslations } from "next-intl";

import { AdminService } from "@/shared/api/connect/client";
import { Button } from "@/shared/ui/button";
import { Card, CardContent, CardHeader, CardTitle } from "@/shared/ui/card";
import { Skeleton } from "@/shared/ui/skeleton";

const METRICS = [
  "usersTotal",
  "tasksTotal",
  "tasksDone",
  "tasksOverdue",
  "mediaTotal",
  "mediaBytes",
  "pushDevices",
  "outboxLagSeconds",
  "jobsPending",
] as const;

/** Operator dashboard: counts and the two health signals (outbox lag, job backlog). */
export function AdminOverview() {
  const t = useTranslations("admin.overview");
  const locale = useLocale();
  const overview = useQuery(
    AdminService.method.getOverview,
    {},
    { refetchInterval: 30_000, meta: { feature: "admin" } },
  );
  const number = new Intl.NumberFormat(locale);
  const bytes = new Intl.NumberFormat(locale, {
    notation: "compact",
    style: "unit",
    unit: "byte",
    unitDisplay: "narrow",
  });

  if (overview.isError) {
    return (
      <div role="alert" className="flex items-center gap-3">
        <p>{t("loadError")}</p>
        <Button variant="outline" onClick={() => overview.refetch()}>
          {t("retry")}
        </Button>
      </div>
    );
  }

  return (
    <dl className="grid grid-cols-2 gap-3 sm:grid-cols-3">
      {METRICS.map((metric) => {
        const value = overview.data?.[metric];
        return (
          <Card key={metric}>
            <CardHeader>
              <CardTitle className="font-normal text-muted-foreground text-sm">
                <dt>{t(`metrics.${metric}`)}</dt>
              </CardTitle>
            </CardHeader>
            <CardContent>
              <dd className="font-semibold text-2xl tabular-nums">
                {value === undefined ? (
                  <Skeleton className="h-8 w-16" />
                ) : metric === "mediaBytes" ? (
                  bytes.format(Number(value))
                ) : (
                  number.format(value)
                )}
              </dd>
            </CardContent>
          </Card>
        );
      })}
    </dl>
  );
}
