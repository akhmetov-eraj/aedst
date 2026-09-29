/// Corner radius scale — mirrors the `--radius-*` custom properties derived
/// from web's `--radius: 0.625rem` base in `apps/web/src/app/styles/globals.css`
/// (`radius-sm = radius * 0.6`, ... `radius-xl = radius * 1.4`).
public enum AedstRadius {
    public static let xs: Double = 4
    public static let sm: Double = 6
    public static let md: Double = 8
    public static let lg: Double = 10
    public static let xl: Double = 14
    public static let xxl: Double = 18
    public static let full: Double = 9999
}
