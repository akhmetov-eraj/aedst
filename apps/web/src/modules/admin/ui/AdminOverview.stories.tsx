import type { Meta, StoryObj } from "@storybook/nextjs-vite";

import { AdminService } from "@/shared/api/connect/generated/aedst/admin/v1/admin_pb";
import { Code, rpc, rpcError } from "@/test/mocks/transport";
import { withAppProviders } from "../../../../.storybook/with-app-providers";
import { AdminOverview } from "./AdminOverview";

const meta = {
  title: "Modules/Admin/AdminOverview",
  component: AdminOverview,
  decorators: [withAppProviders],
  parameters: { roles: ["user", "admin"] },
} satisfies Meta<typeof AdminOverview>;

export default meta;
type Story = StoryObj<typeof meta>;

export const Healthy: Story = {
  parameters: {
    rpc: [
      rpc(AdminService.method.getOverview, () => ({
        usersTotal: 12n,
        tasksTotal: 340n,
        mediaBytes: 5_000_000n,
      })),
    ],
  },
};

export const LoadFailed: Story = {
  parameters: {
    rpc: [rpcError(AdminService.method.getOverview, Code.Internal, "boom")],
  },
};
