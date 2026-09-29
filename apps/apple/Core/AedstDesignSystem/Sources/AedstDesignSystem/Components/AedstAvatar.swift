import SwiftUI

/// Circular user avatar matching shadcn/ui `Avatar` — remote image with a
/// monogram fallback while loading or on failure, so call sites never need
/// their own `AsyncImage` boilerplate.
public struct AedstAvatar: View {
    public enum Size {
        case sm
        case md
        case lg

        var dimension: Double {
            switch self {
            case .sm: 28
            case .md: 40
            case .lg: 56
            }
        }

        var font: Font {
            switch self {
            case .sm: AedstTypography.caption
            case .md: AedstTypography.body
            case .lg: AedstTypography.headline
            }
        }
    }

    private let name: String
    private let imageURL: URL?
    private let size: Size

    public init(name: String, imageURL: URL? = nil, size: Size = .md) {
        self.name = name
        self.imageURL = imageURL
        self.size = size
    }

    private var initials: String {
        let parts = name.split(separator: " ").prefix(2)
        return parts.compactMap(\.first).map(String.init).joined().uppercased()
    }

    public var body: some View {
        ZStack {
            Circle().fill(AedstColor.muted)
            if let imageURL {
                AsyncImage(url: imageURL) { phase in
                    if let image = phase.image {
                        image.resizable().scaledToFill()
                    } else {
                        Text(initials)
                            .font(size.font.weight(.medium))
                            .foregroundStyle(AedstColor.mutedForeground)
                    }
                }
            } else {
                Text(initials)
                    .font(size.font.weight(.medium))
                    .foregroundStyle(AedstColor.mutedForeground)
            }
        }
        .frame(width: size.dimension, height: size.dimension)
        .clipShape(Circle())
        .accessibilityLabel(Text(name))
        .accessibilityAddTraits(.isImage)
    }
}

#Preview("AedstAvatar") {
    HStack(spacing: AedstSpacing.sm) {
        AedstAvatar(name: "Eraj Akhmetov", size: .sm)
        AedstAvatar(name: "Eraj Akhmetov", size: .md)
        AedstAvatar(name: "Eraj Akhmetov", size: .lg)
    }
    .padding()
}
