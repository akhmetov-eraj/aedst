import SwiftUI

/// Custom centered modal matching shadcn/ui `Dialog` — distinct from
/// `AedstSheet` (edge-anchored native presentation); this dims the
/// background and centers a card, closer to web's modal affordance.
public struct AedstDialogModifier<DialogContent: View>: ViewModifier {
    @Binding private var isPresented: Bool
    private let title: String
    private let dialogContent: DialogContent

    public init(isPresented: Binding<Bool>, title: String, @ViewBuilder content: () -> DialogContent) {
        self._isPresented = isPresented
        self.title = title
        self.dialogContent = content()
    }

    public func body(content: Content) -> some View {
        content.overlay {
            if isPresented {
                ZStack {
                    Color.black.opacity(0.4)
                        .ignoresSafeArea()
                        .onTapGesture { isPresented = false }

                    VStack(alignment: .leading, spacing: AedstSpacing.sm) {
                        HStack {
                            Text(title).font(AedstTypography.headline)
                            Spacer()
                            AedstIconButton(systemImage: "xmark", accessibilityLabel: "Close") {
                                isPresented = false
                            }
                        }
                        dialogContent
                    }
                    .padding(AedstSpacing.md)
                    .background(AedstColor.popover)
                    .foregroundStyle(AedstColor.popoverForeground)
                    .clipShape(RoundedRectangle(cornerRadius: AedstRadius.xl, style: .continuous))
                    .shadow(radius: 24, y: 8)
                    .padding(AedstSpacing.lg)
                    .frame(maxWidth: 480)
                }
                .transition(.opacity.combined(with: .scale(scale: 0.96)))
                .accessibilityAddTraits(.isModal)
            }
        }
        .animation(.easeOut(duration: 0.2), value: isPresented)
    }
}

extension View {
    public func aedstDialog<Content: View>(
        isPresented: Binding<Bool>,
        title: String,
        @ViewBuilder content: @escaping () -> Content
    ) -> some View {
        modifier(AedstDialogModifier(isPresented: isPresented, title: title, content: content))
    }
}

#Preview("AedstDialog") {
    @Previewable @State var isPresented = true
    Color.clear
        .aedstDialog(isPresented: $isPresented, title: "Edit profile") {
            Text("Make changes to your profile here.")
                .font(AedstTypography.body)
                .foregroundStyle(AedstColor.mutedForeground)
        }
}
