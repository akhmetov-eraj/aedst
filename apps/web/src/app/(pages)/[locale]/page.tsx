import { useTranslations } from "next-intl";

import { LoginForm } from "@/modules/home/ui/login-form";

export default function HomePage() {
  const t = useTranslations("home");

  return (
    <>
      <h1>{t("title")}</h1>
      <p>{t("description")}</p>

      <LoginForm />
    </>
  );
}
